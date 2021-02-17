#pragma once
#include <stack>
#include <sstream>
#include <fstream>
#include <unordered_map>

#include "vulkan/vulkan.h"

#include <Primitives.h>
#include <Transform.h>
#include <VulkanDevice.h>
#include <VulkanDebug.h>


struct BufferDedicated
{
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
};

template <class T>
inline void hash_combine(std::size_t& s, const T& v)
{
  std::hash<T> h;
  s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
}

template <class T>
class VertexHash;

template<>
struct VertexHash<Vertex>
{
  std::size_t operator()(const Vertex& v) const
  {
    std::size_t res = 0;
    hash_combine(res, v.pos.x);
    hash_combine(res, v.pos.y);
    hash_combine(res, v.pos.z);
    hash_combine(res, v.normal.x);
    hash_combine(res, v.normal.y);
    hash_combine(res, v.normal.z);
    return res;
  }
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
  std::string integratorName = "raytracer";
  uint32_t lightsamples = 1;
  bool lightstratify = false;
  uint32_t samplesPerPixel = 1;

  std::vector<DirectionLight> directLights;
  std::vector<PointLight> pointLights;
  std::vector<QuadLight> quadLights;

  std::vector<Sphere> spheres;
  std::vector<Aabb> aabbs;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  std::vector<Material> triangleMaterials;
  std::vector<Material> sphereMaterials;

  BufferDedicated verticesBuf, indicesBuf, spheresBuf, aabbsBuf, pointLightsBuf,
    directLightsBuf, triangleMaterialsBuf, sphereMaterialsBuf, quadLightsBuf;

  void loadScene(const std::string& filename);
  void loadVulkanBuffersForScene(const VulkanDebug& vkDebug, vks::VulkanDevice* device, VkQueue transferQueue);

private:
  void createBuffer(vks::VulkanDevice* device,
    VkCommandBuffer cmdBuf,
    VkBuffer* buffer,
    VkDeviceMemory* memory,
    VkDeviceSize size_,
    void* data_,
    VkBufferUsageFlags     usage_,
    VkMemoryPropertyFlags  memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  uint32_t addToVertices(const Vertex& v);

private:
  VulkanDebug vkDebug;
  std::vector<BufferDedicated> m_stagingBuffers;
  std::unordered_map<Vertex, uint32_t, VertexHash<Vertex>> verticesMap;
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
