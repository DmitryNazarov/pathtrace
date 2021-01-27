#include <iostream>
#include <unordered_set>
#include <array>
#include <SceneLoader.h>


uint32_t addToVertices(std::vector<Vertex>& vertices, const Vertex& v)
{
  auto it = std::find(vertices.begin(), vertices.end(), v);
  if (it != vertices.end())
  {
    return std::distance(vertices.begin(), it);
  }
  vertices.push_back(v);
  return vertices.size() - 1;
}

Aabb calcAabb(const Sphere& s)
{
  std::array<vec3, 8> AabbPoints {
    s.pos - vec3(s.radius),
    vec3(s.pos.x - s.radius, s.pos.y + s.radius, s.pos.z - s.radius),
    vec3(s.pos.x + s.radius, s.pos.y + s.radius, s.pos.z - s.radius),
    vec3(s.pos.x + s.radius, s.pos.y - s.radius, s.pos.z - s.radius),
    vec3(s.pos.x - s.radius, s.pos.y - s.radius, s.pos.z + s.radius),
    vec3(s.pos.x - s.radius, s.pos.y + s.radius, s.pos.z + s.radius),
    vec3(s.pos.x + s.radius, s.pos.y - s.radius, s.pos.z + s.radius),
    s.pos + vec3(s.radius)
  };

  for (auto &i : AabbPoints)
  {
    i = s.transform * vec4(i, 1.0f);
  }

  vec3 min(AabbPoints[0]), max(AabbPoints[0]);
  for (size_t i = 1; i < AabbPoints.size(); ++i)
  {
    float& x = AabbPoints[i].x;
    float& y = AabbPoints[i].y;
    float& z = AabbPoints[i].z;

    if (x < min.x)
      min.x = x;
    else if (x > max.x)
      max.x = x;
    if (y < min.y)
      min.y = y;
    else if (y > max.y)
      max.y = y;
    if (z < min.z)
      min.z = z;
    else if (z > max.z)
      max.z = z;
  }

  return {min, max};
}

void Scene::loadScene(const std::string& filename)
{
  std::vector<vec3> sceneVertices;
  std::vector<std::pair<vec3, vec3>> vertexNormals;

  vec4 ambient{ 0.2f, 0.2f, 0.2f, 1.0f };
  vec4 diffuse{ 0.0f, 0.0f, 0.0f, 0.0f };
  vec4 specular{ 0.0f, 0.0f, 0.0f, 0.0f };
  vec4 emission{ 0.0f, 0.0f, 0.0f, 0.0f };
  float shininess = .0;
  vec3 attenuation{ 1.0f, 0.0f, 0.0f };

  std::string str, cmd;
  std::ifstream in(filename);
  if (!in.is_open())
    return;

  std::stack<mat4> transfstack;
  transfstack.push(mat4(1.0)); // identity

  while (getline(in, str)) {
    if ((str.find_first_not_of(" \t\r\n") == std::string::npos) ||
      (str[0] == '#'))
      continue;

    std::stringstream ss(str);
    ss >> cmd;

    if (cmd == "size") {
      int values[2];
      if (readvals(ss, 2, values)) {
        width = values[0];
        height = values[1];
        aspect = static_cast<float>(width) / static_cast<float>(height);
      }
    }
    else if (cmd == "camera") {
      float values[10];
      if (readvals(ss, 10, values)) {
        eyeInit = vec3(values[0], values[1], values[2]);
        center = vec3(values[3], values[4], values[5]);
        upInit = vec3(values[6], values[7], values[8]);
        fovy = values[9];
      }
    }
    else if (cmd == "maxdepth") {
      int value;
      if (readvals(ss, 1, &value)) {
        depth = value;
      }
    }
    else if (cmd == "output") {
      std::string value;
      if (readvals(ss, 1, &value)) {
        screenshotName = value;
      }
    }
    else if (cmd == "sphere") {
      float values[4];
      if (readvals(ss, 4, values)) {
        Sphere s;
        s.pos = vec3(values[0], values[1], values[2]);
        s.radius = values[3];
        s.transform = transfstack.top();
        s.invertedTransform = inverse(transfstack.top());
        spheres.push_back(s);
        aabbs.push_back(calcAabb(s));
        sphereMaterials.emplace_back(ambient, diffuse, specular, emission, shininess);
      }
    }
    else if (cmd == "translate") {
      float values[3];
      if (readvals(ss, 3, values)) {
        transfstack.top() = translate(transfstack.top(), vec3(values[0], values[1], values[2]));
      }
    }
    else if (cmd == "scale") {
      float values[3];
      if (readvals(ss, 3, values)) {
        transfstack.top() = scale(transfstack.top(), vec3(values[0], values[1], values[2]));
      }
    }
    else if (cmd == "rotate") {
      float values[4];
      if (readvals(ss, 4, values)) {
        vec3 axis = normalize(vec3(values[0], values[1], values[2]));
        transfstack.top() = rotate(transfstack.top(), radians(values[3]), axis);
      }
    }
    else if (cmd == "pushTransform") {
      transfstack.push(transfstack.top());
    }
    else if (cmd == "popTransform") {
      if (transfstack.size() <= 1) {
        std::cerr << "Stack has no elements.  Cannot Pop\n";
      }
      else {
        transfstack.pop();
      }
    }
    else if (cmd == "vertex") {
      float values[3];
      if (readvals(ss, 3, values)) {
        sceneVertices.emplace_back(values[0], values[1], values[2]);
      }
    }
    else if (cmd == "vertexnormal") {
      float values[6];
      if (readvals(ss, 6, values)) {
        auto vertex = vec3(values[0], values[1], values[2]);
        auto vertexNormal = vec3(values[3], values[4], values[5]);
        vertexNormals.emplace_back(vertex, vertexNormal);
      }
    }
    else if (cmd == "tri") {
      int values[3];
      if (readvals(ss, 3, values)) {
        vec3 pos0 = transfstack.top() * vec4(sceneVertices[values[0]], 1.0f);
        vec3 pos1 = transfstack.top() * vec4(sceneVertices[values[1]], 1.0f);
        vec3 pos2 = transfstack.top() * vec4(sceneVertices[values[2]], 1.0f);
        vec3 normal = normalize(cross(pos1 - pos0, pos2 - pos0));
        uint32_t index0 = addToVertices(vertices, Vertex(pos0, normal));
        uint32_t index1 = addToVertices(vertices, Vertex(pos1, normal));
        uint32_t index2 = addToVertices(vertices, Vertex(pos2, normal));
        indices.push_back(index0);
        indices.push_back(index1);
        indices.push_back(index2);
        triangleMaterials.emplace_back(ambient, diffuse, specular, emission, shininess);
      }
    }
    else if (cmd == "directional") {
      float values[6];
      if (readvals(ss, 6, values)) {
        auto dir = vec3(values[0], values[1], values[2]);
        auto c = vec4(values[3], values[4], values[5], 1.0f);
        directLights.emplace_back(dir, c);
      }
    }
    else if (cmd == "point") {
      float values[6];
      if (readvals(ss, 6, values)) {
        auto pos = vec3(values[0], values[1], values[2]);
        auto c = vec4(values[3], values[4], values[5], 1.0f);
        pointLights.emplace_back(pos, c, attenuation);
      }
    }
    else if (cmd == "ambient") {
      float values[3];
      if (readvals(ss, 3, values)) {
        ambient = vec4(values[0], values[1], values[2], 1.0f);
      }
    }
    else if (cmd == "attenuation") {
      float values[3];
      if (readvals(ss, 3, values)) {
        attenuation[0] = values[0];
        attenuation[1] = values[1];
        attenuation[2] = values[2];
      }
    }
    else if (cmd == "diffuse") {
      float values[3];
      if (readvals(ss, 3, values)) {
        diffuse = vec4(values[0], values[1], values[2], 1.0f);
      }
    }
    else if (cmd == "specular") {
      float values[3];
      if (readvals(ss, 3, values)) {
        specular = vec4(values[0], values[1], values[2], 1.0f);
      }
    }
    else if (cmd == "emission") {
      float values[3];
      if (readvals(ss, 3, values)) {
        emission = vec4(values[0], values[1], values[2], 1.0f);
      }
    }
    else if (cmd == "shininess") {
      float value;
      if (readvals(ss, 1, &value)) {
        shininess = value;
      }
    }
    else if (cmd == "maxverts" || cmd == "maxvertnorms") {
      continue;
    }
    else if (cmd == "quadLight")  // quadLight <a> <ab> <ac> <intensity>
    {
      float values[12];
      if (readvals(ss, 12, values)) {
        auto pos = vec3(values[0], values[1], values[2]);
        auto abSide = vec3(values[3], values[4], values[5]);
        auto acSide = vec3(values[6], values[7], values[8]);
        auto color = vec4(values[9], values[10], values[11], 1.0f);
        quadLights.emplace_back(pos, abSide, acSide, color);
      }
    }
    else if (cmd == "integrator")  // integrator <name>
    {
      std::string value;
      if (readvals(ss, 1, &value)) {
        integratorName = value;
      }
    }
    else {
      std::cerr << "Unknown Command: " << cmd << " Skipping \n";
    }
  }
}

// Staging buffer creation, uploading data to device buffer
void Scene::createBuffer(vks::VulkanDevice* device,
  VkCommandBuffer cmdBuf,
  VkBuffer* buffer,
  VkDeviceMemory* memory,
  VkDeviceSize size_,
  void* data_,
  VkBufferUsageFlags     usage_,
  VkMemoryPropertyFlags  memProps)
{
  m_stagingBuffers.emplace_back();
  BufferDedicated& staging = m_stagingBuffers.back();

  // Create staging buffer
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    size_,
    &staging.buffer,
    &staging.memory,
    data_));

  // Create device local buffer
  VK_CHECK_RESULT(device->createBuffer(
    usage_,
    memProps,
    size_,
    buffer,
    memory));

  // Copy from staging buffer (need to submit command buffer, flushStaging must be done after submitting)
  VkBufferCopy region{ 0, 0, size_ };
  vkCmdCopyBuffer(cmdBuf, staging.buffer, *buffer, 1, &region);
}

void Scene::loadVulkanBuffersForScene(const VulkanDebug& vkDebug, vks::VulkanDevice* device, VkQueue transferQueue)
{
  VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

  if (!vertices.empty())
  {
    createBuffer(device, copyCmd, &verticesBuf.buffer, &verticesBuf.memory, vertices.size() * sizeof(Vertex), vertices.data(),
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    vkDebug.setBufferName(verticesBuf.buffer, "Vertices");
  }

  if (!indices.empty())
  {
    createBuffer(device, copyCmd, &indicesBuf.buffer, &indicesBuf.memory, indices.size() * sizeof(uint32_t), indices.data(),
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    vkDebug.setBufferName(indicesBuf.buffer, "Indices");
  }

  if (!spheres.empty())
  {
    createBuffer(device, copyCmd, &spheresBuf.buffer, &spheresBuf.memory, spheres.size() * sizeof(Sphere), spheres.data(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vkDebug.setBufferName(spheresBuf.buffer, "Spheres");
  }

  if (!aabbs.empty())
  {
    createBuffer(device, copyCmd, &aabbsBuf.buffer, &aabbsBuf.memory, aabbs.size() * sizeof(Aabb), aabbs.data(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    vkDebug.setBufferName(aabbsBuf.buffer, "Aabbs");
  }

  if (!pointLights.empty())
  {
    createBuffer(device, copyCmd, &pointLightsBuf.buffer, &pointLightsBuf.memory, pointLights.size() * sizeof(PointLight), pointLights.data(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vkDebug.setBufferName(pointLightsBuf.buffer, "PointLights");
  }

  if (!directLights.empty())
  {
    createBuffer(device, copyCmd, &directLightsBuf.buffer, &directLightsBuf.memory, directLights.size() * sizeof(DirectionLight), directLights.data(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vkDebug.setBufferName(directLightsBuf.buffer, "DirectLights");
  }

  if (!triangleMaterials.empty())
  {
    createBuffer(device, copyCmd, &triangleMaterialsBuf.buffer, &triangleMaterialsBuf.memory, triangleMaterials.size() * sizeof(Material), triangleMaterials.data(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vkDebug.setBufferName(triangleMaterialsBuf.buffer, "TriangleMaterials");
  }

  if (!sphereMaterials.empty())
  {
    createBuffer(device, copyCmd, &sphereMaterialsBuf.buffer, &sphereMaterialsBuf.memory, sphereMaterials.size() * sizeof(Material), sphereMaterials.data(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vkDebug.setBufferName(sphereMaterialsBuf.buffer, "SphereMaterials");
  }

  if (!quadLights.empty())
  {
    createBuffer(device, copyCmd, &quadLightsBuf.buffer, &quadLightsBuf.memory, quadLights.size() * sizeof(QuadLight), quadLights.data(),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vkDebug.setBufferName(quadLightsBuf.buffer, "QuadLights");
  }

  device->flushCommandBuffer(copyCmd, transferQueue, true);

  for (auto& i : m_stagingBuffers)
  {
    vkDestroyBuffer(device->logicalDevice, i.buffer, VK_NULL_HANDLE);
    vkFreeMemory(device->logicalDevice, i.memory, VK_NULL_HANDLE);
  }
}
