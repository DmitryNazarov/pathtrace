#pragma once
#include <stack>
#include <sstream>
#include <fstream>

#include "vulkan/vulkan.h"

#include <Primitives.h>
#include <Transform.h>
#include <VulkanDevice.h>


struct VArray
{
  uint32_t count;
  VkBuffer buffer;
  VkDeviceMemory memory;
};

struct Vertex
{
  Vertex(const glm::vec3& pos, const glm::vec3& normal) : pos(pos), normal(normal)
  {}
  alignas(16) glm::vec3 pos;
  alignas(16) glm::vec3 normal;
};

struct Scene
{
  size_t width = 640, height = 480;
  float aspect;
  int depth = 5;
  std::string filename = "screenshot.png";
  vec3 eye_init;
  vec3 center;
  vec3 up_init;
  float fovy = 90;

  vec3 w, u, v;

  std::vector<Sphere> spheres;

  std::vector<DirectionLight> directLights;
  std::vector<PointLight> pointLights;

  std::vector<Material> triangleMaterials;
  std::vector<Material> sphereMaterials;

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  VArray verticesBuf, indicesBuf, pointLightsBuf, directLightsBuf, triangleMaterialsBuf, sphereMaterialsBuf;
  void loadVulkanBuffersForScene(vks::VulkanDevice* device, VkQueue transferQueue, VkMemoryPropertyFlags memoryPropertyFlags = 0);
};

template <typename T>
bool readvals(std::stringstream& s, const int numvals, T* values)
{
  for (int i = 0; i < numvals; ++i) {
    s >> values[i];
    if (s.fail()) {
      std::cout << "Failed reading value " << i << " will skip\n";
      return false;
    }
  }
  return true;
}

Scene loadScene(const std::string& filename);