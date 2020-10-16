#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <transform.h>

using namespace Transform;
using Color = vec4;

struct Material {
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  vec4 emission;
  float shininess;
};

struct Sphere {
  float radius;
  vec3 pos;
  Material material;
  mat4 transform{1.0f}, inverted_transform{ 1.0f };
};

struct Triangle {
  std::vector<vec3> vertices;
  vec3 normal;
  Material material;
};

struct TriangleNormals {
  std::vector<vec3> vertices;
  std::vector<vec3> normals;
  Material material;
};

struct DirectionLight {
  DirectionLight(const vec3& dir, const Color& c) : dir(dir), color(c) {}
  vec3 dir;
  Color color;
};

struct PointLight {
  PointLight(const vec3& pos, const Color& c, const vec3& attenuation) :
    pos(pos), color(c), attenuation(attenuation)
  {}

  vec3 pos;
  Color color;
  vec3 attenuation{ 1.0f, 0.0f, 0.0f };
};

struct Ray {
  Ray(const vec3& origin, const vec3& direction)
    : orig(origin), dir(direction) {}
  vec3 orig;
  vec3 dir;
};

#endif // PRIMITIVES_H
