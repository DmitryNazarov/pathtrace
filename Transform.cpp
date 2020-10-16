#include "Transform.h"

namespace Transform {
mat4 rotate(const mat4 &m, float radians, const vec3 &axis) {
  float cos_a = cos(radians);
  float sin_a = sin(radians);

  const float &x = axis[0];
  const float &y = axis[1];
  const float &z = axis[2];

  mat3 m2 = mat3(x * x, x * y, x * z, x * y, y * y, y * z, x * z, y * z, z * z);
  mat3 m3 = mat3(0., z, -y, -z, 0., x, y, -x, 0.);

  mat3 rotate = mat3(1.0) * cos_a + m2 * (1 - cos_a) + m3 * sin_a;

  return m * mat4(rotate);
}

mat4 scale(const mat4 &m, const vec3 &v) {
  mat4 scale = mat4(v.x, 0, 0, 0, 0, v.y, 0, 0, 0, 0, v.z, 0, 0, 0, 0, 1);
  return m * scale;
}

mat4 translate(const mat4 &m, const vec3 &v) {
  mat4 translate = mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, v.x, v.y, v.z, 1);
  return m * translate;
}

float dot(const vec3 &a, const vec3 &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

float radians(float angle) { return angle * static_cast<float>(M_PI) / 180.0f; }

float length(const vec3 &v) { return sqrt(dot(v, v)); }

vec3 normalize(const vec3 &v) {
  return v / length(v);
}

vec3 cross(const vec3 &a, const vec3 &b) {
  float x = a.y * b.z - a.z * b.y;
  float y = a.z * b.x - a.x * b.z;
  float z = a.x * b.y - a.y * b.x;
  return vec3(x, y, z);
}

vec3 reflect(const vec3 &incident, const vec3 &normal) {
  return incident - 2.0f * normal * dot(normal, incident);
}

template <> float determinant(const mat<2> &m) {
  return m[0][0] * m[1][1] - m[1][0] * m[0][1];
}

std::string debug_vector(const vec3 &v1, const vec3 &v2) {
  std::ostringstream ss;
  ss << "vector left: " << std::setw(4) << "\n";
  ss << v1.x << " " << v1.y << " " << v1.z << "\n";
  ss << "vector right: " << std::setw(4) << "\n";
  ss << v2.x << " " << v2.y << " " << v2.z << "\n";
  return ss.str();
}

}; // namespace Transform