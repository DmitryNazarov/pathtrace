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
    else {
      std::cerr << "Unknown Command: " << cmd << " Skipping \n";
    }
  }
}

void Scene::loadVulkanBuffersForScene(const VulkanDebug& vkDebug, vks::VulkanDevice* device, VkQueue transferQueue)
{
  verticesBuf.count = vertices.size();
  indicesBuf.count = indices.size();
  spheresBuf.count = spheres.size();
  aabbsBuf.count = aabbs.size();
  pointLightsBuf.count = pointLights.size();
  directLightsBuf.count = directLights.size();
  triangleMaterialsBuf.count = triangleMaterials.size();
  sphereMaterialsBuf.count = sphereMaterials.size();
  size_t vertexSize = vertices.size() * sizeof(Vertex);
  size_t indexSize = indices.size() * sizeof(uint32_t);
  size_t spheresSize = spheres.size() * sizeof(Sphere);
  size_t aabbsSize = aabbs.size() * sizeof(Aabb);
  size_t pointLightsSize = pointLights.size() * sizeof(PointLight);
  size_t directLightsSize = directLights.size() * sizeof(DirectionLight);
  size_t triangleMaterialsSize = triangleMaterials.size() * sizeof(Material);
  size_t sphereMaterialsSize = sphereMaterials.size() * sizeof(Material);
  if (!vertexSize) vertexSize = 1; //fix it
  if (!indexSize) indexSize = 1; //fix it
  if (!spheresSize) spheresSize = aabbsSize = 1; //fix it
  if (!triangleMaterialsSize) triangleMaterialsSize = 1; //fix it
  if (!sphereMaterialsSize) sphereMaterialsSize = 1; //fix it
  if (!directLightsSize) directLightsSize = 1; //fix it
  if (!pointLightsSize) pointLightsSize = 1; //fix it
  if (!pointLightsSize) pointLightsSize = 1; //fix it

  struct StagingBuffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
  };
  StagingBuffer vertexStaging, indexStaging,
    sphereStaging, aabbStaging,
    pointLightsStaging, directLightsStaging,
    triangleMaterialsStaging, sphereMaterialsStaging;

  // Create staging buffers
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    vertexSize,
    &vertexStaging.buffer,
    &vertexStaging.memory,
    vertices.data()));
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    indexSize,
    &indexStaging.buffer,
    &indexStaging.memory,
    indices.data()));
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    spheresSize,
    &sphereStaging.buffer,
    &sphereStaging.memory,
    spheres.data()));
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    aabbsSize,
    &aabbStaging.buffer,
    &aabbStaging.memory,
    aabbs.data()));
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    pointLightsSize,
    &pointLightsStaging.buffer,
    &pointLightsStaging.memory,
    pointLights.data()));
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    directLightsSize,
    &directLightsStaging.buffer,
    &directLightsStaging.memory,
    directLights.data()));
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    triangleMaterialsSize,
    &triangleMaterialsStaging.buffer,
    &triangleMaterialsStaging.memory,
    triangleMaterials.data()));
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    sphereMaterialsSize,
    &sphereMaterialsStaging.buffer,
    &sphereMaterialsStaging.memory,
    sphereMaterials.data()));

  // Create device local buffers
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    vertexSize,
    &verticesBuf.buffer,
    &verticesBuf.memory));
  vkDebug.setBufferName(verticesBuf.buffer, "Vertices");

  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    indexSize,
    &indicesBuf.buffer,
    &indicesBuf.memory));
  vkDebug.setBufferName(indicesBuf.buffer, "Indices");

  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    spheresSize,
    &spheresBuf.buffer,
    &spheresBuf.memory));
  vkDebug.setBufferName(spheresBuf.buffer, "Spheres");

  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    aabbsSize,
    &aabbsBuf.buffer,
    &aabbsBuf.memory));
  vkDebug.setBufferName(aabbsBuf.buffer, "Aabbs");

  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    pointLightsSize,
    &pointLightsBuf.buffer,
    &pointLightsBuf.memory));
  vkDebug.setBufferName(pointLightsBuf.buffer, "PointLights");

  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    directLightsSize,
    &directLightsBuf.buffer,
    &directLightsBuf.memory));
  vkDebug.setBufferName(directLightsBuf.buffer, "DirectLights");

  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    triangleMaterialsSize,
    &triangleMaterialsBuf.buffer,
    &triangleMaterialsBuf.memory));
  vkDebug.setBufferName(triangleMaterialsBuf.buffer, "TriangleMaterials");

  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    sphereMaterialsSize,
    &sphereMaterialsBuf.buffer,
    &sphereMaterialsBuf.memory));
  vkDebug.setBufferName(sphereMaterialsBuf.buffer, "SphereMaterials");

  // Copy from staging buffers
  VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

  VkBufferCopy copyRegion = {};

  copyRegion.size = vertexSize;
  vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, verticesBuf.buffer, 1, &copyRegion);

  copyRegion.size = indexSize;
  vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indicesBuf.buffer, 1, &copyRegion);

  copyRegion.size = spheresSize;
  vkCmdCopyBuffer(copyCmd, sphereStaging.buffer, spheresBuf.buffer, 1, &copyRegion);

  copyRegion.size = aabbsSize;
  vkCmdCopyBuffer(copyCmd, aabbStaging.buffer, aabbsBuf.buffer, 1, &copyRegion);

  copyRegion.size = pointLightsSize;
  vkCmdCopyBuffer(copyCmd, pointLightsStaging.buffer, pointLightsBuf.buffer, 1, &copyRegion);

  copyRegion.size = directLightsSize;
  vkCmdCopyBuffer(copyCmd, directLightsStaging.buffer, directLightsBuf.buffer, 1, &copyRegion);

  copyRegion.size = triangleMaterialsSize;
  vkCmdCopyBuffer(copyCmd, triangleMaterialsStaging.buffer, triangleMaterialsBuf.buffer, 1, &copyRegion);

  copyRegion.size = sphereMaterialsSize;
  vkCmdCopyBuffer(copyCmd, sphereMaterialsStaging.buffer, sphereMaterialsBuf.buffer, 1, &copyRegion);

  device->flushCommandBuffer(copyCmd, transferQueue, true);

  vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, VK_NULL_HANDLE);
  vkFreeMemory(device->logicalDevice, vertexStaging.memory, VK_NULL_HANDLE);
  vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, VK_NULL_HANDLE);
  vkFreeMemory(device->logicalDevice, indexStaging.memory, VK_NULL_HANDLE);
  vkDestroyBuffer(device->logicalDevice, sphereStaging.buffer, VK_NULL_HANDLE);
  vkFreeMemory(device->logicalDevice, sphereStaging.memory, VK_NULL_HANDLE);
  vkDestroyBuffer(device->logicalDevice, aabbStaging.buffer, VK_NULL_HANDLE);
  vkFreeMemory(device->logicalDevice, aabbStaging.memory, VK_NULL_HANDLE);
  vkDestroyBuffer(device->logicalDevice, pointLightsStaging.buffer, VK_NULL_HANDLE);
  vkFreeMemory(device->logicalDevice, pointLightsStaging.memory, VK_NULL_HANDLE);
  vkDestroyBuffer(device->logicalDevice, directLightsStaging.buffer, VK_NULL_HANDLE);
  vkFreeMemory(device->logicalDevice, directLightsStaging.memory, VK_NULL_HANDLE);
  vkDestroyBuffer(device->logicalDevice, triangleMaterialsStaging.buffer, VK_NULL_HANDLE);
  vkFreeMemory(device->logicalDevice, triangleMaterialsStaging.memory, VK_NULL_HANDLE);
  vkDestroyBuffer(device->logicalDevice, sphereMaterialsStaging.buffer, VK_NULL_HANDLE);
  vkFreeMemory(device->logicalDevice, sphereMaterialsStaging.memory, VK_NULL_HANDLE);
}
