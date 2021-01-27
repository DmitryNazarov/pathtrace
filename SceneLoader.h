#pragma once
#include <stack>
#include <sstream>
#include <fstream>

#include "vulkan/vulkan.h"

#include <Primitives.h>
#include <Transform.h>
#include <VulkanDevice.h>
#include <VulkanDebug.h>


struct VArray
{
  uint32_t count;
  VkBuffer buffer;
  VkDeviceMemory memory;
};

class Scene
{
public:
  size_t width = 640, height = 480;
  float aspect;
  uint32_t depth = 5;
  std::string screenshotName = "screenshot.png";
  vec3 eyeInit;
  vec3 center;
  vec3 upInit;
  float fovy = 90;

  std::vector<DirectionLight> directLights;
  std::vector<PointLight> pointLights;

  std::vector<Sphere> spheres;
  std::vector<Aabb> aabbs;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  std::vector<Material> triangleMaterials;
  std::vector<Material> sphereMaterials;

  VArray verticesBuf, indicesBuf, spheresBuf, aabbsBuf, pointLightsBuf, directLightsBuf, triangleMaterialsBuf, sphereMaterialsBuf;

  void loadScene(const std::string& filename);
  void loadVulkanBuffersForScene(const VulkanDebug& vkDebug, vks::VulkanDevice* device, VkQueue transferQueue);

private:
  VulkanDebug vkDebug;
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
