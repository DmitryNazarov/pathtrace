#include <iostream>
#include <unordered_set>
#include <SceneLoader.h>


uint32_t addToVertices(std::vector<Vertex>& vertices, const vec3& pos, const vec3& normal)
{
  auto it = std::find_if(vertices.begin(), vertices.end(), [&pos](const Vertex& v) {
    return v.pos == pos;
  });
  if (it != vertices.end())
  {
    return std::distance(vertices.begin(), it);
  }
  vertices.emplace_back(pos, normal);
  return vertices.size() - 1;
}

Scene loadScene(const std::string& filename) {
  Scene scene;

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
    return scene;

  std::stack<mat4> transfstack;
  transfstack.push(mat4(1.0)); // identity

  //minus all y and z coords because of different coordinate frames in scene(ogl) and in raytracer(vulcan)
  while (getline(in, str)) {
    if ((str.find_first_not_of(" \t\r\n") == std::string::npos) ||
      (str[0] == '#'))
      continue;

    std::stringstream ss(str);
    ss >> cmd;

    if (cmd == "size") {
      int values[2];
      if (readvals(ss, 2, values)) {
        scene.width = values[0];
        scene.height = values[1];
        scene.aspect = static_cast<float>(scene.width) /
          static_cast<float>(scene.height);
      }
    }
    else if (cmd == "camera") {
      float values[10];
      if (readvals(ss, 10, values)) {
        scene.eye_init = vec3(values[0], -values[1], -values[2]);
        scene.center = vec3(values[3], -values[4], -values[5]);
        scene.up_init = vec3(values[6], -values[7], -values[8]);
        scene.fovy = values[9];

        scene.w = normalize(scene.eye_init - scene.center);
        scene.u = normalize(cross(scene.up_init, scene.w));
        scene.v = cross(scene.w, scene.u);
      }
    }
    else if (cmd == "maxdepth") {
      int value;
      if (readvals(ss, 1, &value)) {
        scene.depth = value;
      }
    }
    else if (cmd == "output") {
      std::string value;
      if (readvals(ss, 1, &value)) {
        scene.filename = value;
      }
    }
    else if (cmd == "sphere") {
      continue; //fix it

      //float values[4];
      //if (readvals(ss, 4, values)) {
      //  Sphere s;
      //  s.radius = values[3];
      //  s.pos = vec3(values[0], values[1], values[2]);
      //  s.transform = transfstack.top();
      //  s.inverted_transform = inverse(transfstack.top());
      //  s.material.ambient = ambient;
      //  s.material.diffuse = diffuse;
      //  s.material.specular = specular;
      //  s.material.emission = emission;
      //  s.material.shininess = shininess;
      //  settings.spheres.push_back(s);
      //  Object o;
      //  o.type = SPHERE;
      //  o.index = settings.spheres.size() - 1;
      //  settings.objects.push_back(o);
      //}
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
        sceneVertices.emplace_back(values[0], -values[1], -values[2]);
      }
    }
    else if (cmd == "vertexnormal") {
      float values[6];
      if (readvals(ss, 6, values)) {
        auto vertex = vec3(values[0], -values[1], -values[2]);
        auto vertexNormal = vec3(values[3], -values[4], -values[5]);
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
        uint32_t index0 = addToVertices(scene.vertices, pos0, normal);
        uint32_t index1 = addToVertices(scene.vertices, pos1, normal);
        uint32_t index2 = addToVertices(scene.vertices, pos2, normal);
        scene.indices.push_back(index0);
        scene.indices.push_back(index1);
        scene.indices.push_back(index2);
        scene.triangleMaterials.emplace_back(ambient, diffuse, specular, emission, shininess);
      }
    }
    //else if (cmd == "trinormal") {
    //  int values[3];
    //  if (readvals(ss, 3, values)) {
    //    TriangleNormals t;
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[0]].first, 1.0f));
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[1]].first, 1.0f));
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[2]].first, 1.0f));
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[0]].second, 1.0f));
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[1]].second, 1.0f));
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[2]].second, 1.0f));
    //    t.material.ambient = ambient;
    //    t.material.diffuse = diffuse;
    //    t.material.specular = specular;
    //    t.material.emission = emission;
    //    t.material.shininess = shininess;
    //    settings.triangle_normals.push_back(t);
    //    Object o;
    //    o.type = TRIANGLE_NORMALS;
    //    o.index = settings.triangle_normals.size() - 1;
    //    settings.objects.push_back(o);
    //  }
    //}
    else if (cmd == "directional") {
      float values[6];
      if (readvals(ss, 6, values)) {
        auto dir = vec3(values[0], -values[1], -values[2]);
        auto c = vec4(values[3], values[4], values[5], 1.0f);
        scene.directLights.emplace_back(dir, c);
      }
    }
    else if (cmd == "point") {
      float values[6];
      if (readvals(ss, 6, values)) {
        auto pos = vec3(values[0], -values[1], -values[2]);
        auto c = vec4(values[3], values[4], values[5], 1.0f);
        scene.pointLights.emplace_back(pos, c, attenuation);
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

  return scene;
}

void Scene::loadVulkanBuffersForScene(vks::VulkanDevice* device, VkQueue transferQueue, VkMemoryPropertyFlags memoryPropertyFlags)
{
  verticesBuf.count = vertices.size();
  indicesBuf.count = indices.size();
  size_t vertexSize = vertices.size() * sizeof(Vertex);
  size_t indexSize = indices.size() * sizeof(uint32_t);

  struct StagingBuffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
  } vertexStaging, indexStaging;

  // Create staging buffers
  // Vertex data
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    vertexSize,
    &vertexStaging.buffer,
    &vertexStaging.memory,
    vertices.data()));
  // Index data
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    indexSize,
    &indexStaging.buffer,
    &indexStaging.memory,
    indices.data()));

  // Create device local buffers
  // Vertex buffer
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | memoryPropertyFlags,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    vertexSize,
    &verticesBuf.buffer,
    &verticesBuf.memory));
  // Index buffer
  VK_CHECK_RESULT(device->createBuffer(
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | memoryPropertyFlags,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    indexSize,
    &indicesBuf.buffer,
    &indicesBuf.memory));

  // Copy from staging buffers
  VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

  VkBufferCopy copyRegion = {};

  copyRegion.size = vertexSize;
  vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, verticesBuf.buffer, 1, &copyRegion);

  copyRegion.size = indexSize;
  vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indicesBuf.buffer, 1, &copyRegion);

  device->flushCommandBuffer(copyCmd, transferQueue, true);

  vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
  vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
  vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
  vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);
}
