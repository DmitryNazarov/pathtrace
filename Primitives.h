#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <transform.h>

using namespace Transform;

struct Material {
  Material(vec4 ambient,
    vec4 diffuse,
    vec4 specular,
    vec4 emission,
    float shininess) :
    ambient(ambient),
    diffuse(diffuse),
    specular(specular),
    emission(emission),
    shininess(shininess)
  {
  }
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  vec4 emission;
  // Vulkan alignment requirements:
  // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
  alignas(16) float shininess;
};

struct Sphere {
  float radius;
  vec3 pos;
  Material material;
  mat4 transform{1.0f}, inverted_transform{ 1.0f };
};

struct DirectionLight {
  DirectionLight(const vec3& dir, const vec4& color) : dir(dir), color(color) {}
  alignas(16) vec3 dir;
  alignas(16) vec4 color;
};

struct PointLight {
  PointLight(const vec3& pos, const vec4& color, const vec3& attenuation) :
    pos(pos), color(color), attenuation(attenuation)
  {}

  alignas(16) vec3 pos;
  alignas(16) vec4 color;
  alignas(16) vec3 attenuation{ 1.0f, 0.0f, 0.0f };
};

#endif // PRIMITIVES_H
