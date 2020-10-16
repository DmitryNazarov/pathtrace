#pragma once
#include <stack>
#include <sstream>
#include <fstream>

#include <Primitives.h>
#include <Transform.h>

enum ObjectType { TRIANGLE, TRIANGLE_NORMALS, SPHERE };

struct Object {
  ObjectType type;
  std::vector<size_t> indices;
  Material material;
};

struct Settings {
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
  std::vector<Triangle> triangles;
  std::vector<TriangleNormals> triangle_normals;

  std::vector<DirectionLight> direct_lights;
  std::vector<PointLight> point_lights;

  std::vector<Object> objects;

  std::vector<vec3> vertices;
  std::vector<uint32_t> indices;
};

template <typename T>
bool readvals(std::stringstream& s, const int numvals, T* values) {
  for (int i = 0; i < numvals; ++i) {
    s >> values[i];
    if (s.fail()) {
      std::cout << "Failed reading value " << i << " will skip\n";
      return false;
    }
  }
  return true;
}

Settings read_settings(const std::string& filename);