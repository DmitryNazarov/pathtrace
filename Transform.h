#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <type_traits>
#define _USE_MATH_DEFINES
#include <iomanip>
#include <math.h>
#include <sstream>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Transform {

using vec3 = glm::vec3;
using vec4 = glm::vec4;
template <int N> using mat = glm::mat<N, N, glm::f32, glm::defaultp>;
using mat2 = glm::mat2;
using mat3 = glm::mat3;
using mat4 = glm::mat4;

template <int N>
[[nodiscard]] std::string debug_matrix(const mat<N> &t) {
  std::ostringstream ss;
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      ss << std::setw(4) << t[j][i] << " ";
    }
    ss << "\n";
  }

  return ss.str();
}

[[nodiscard]] std::string debug_vector(const vec3 &v1, const vec3 &v2);

mat4 rotate(const mat4 &m, float degrees, const vec3 &axis);
mat4 scale(const mat4 &m, const vec3 &v);
mat4 translate(const mat4 &m, const vec3 &v);
vec3 normalize(const vec3 &v);
vec3 cross(const vec3 &a, const vec3 &b);
float dot(const vec3 &a, const vec3 &b);
float length(const vec3 &v);
float radians(float angle);
vec3 reflect(const vec3 &v, const vec3 &normal);

template <int N>
mat<N - 1> create_submatrix(const mat<N> &m, int skip_column, int skip_row) {
  mat<N - 1> result;
  int _row = 0, _col = 0;
  for (int i = 0; i < N * N; ++i) {
    int row = i % N, col = i / N;
    if (row == skip_row || col == skip_column) {
      continue;
    }

    _row = row;
    _col = col;
    if (row > skip_row) {
      --_row;
    }
    if (col > skip_column) {
      --_col;
    }

    result[_col][_row] = m[col][row];
  }

  return result;
}

template <int N> float determinant(const mat<N> &m) {
  float result = 0.f;
  for (int i = 0; i < N; ++i) {
    float sign = powf(-1, static_cast<float>(i));
    auto sub_m = create_submatrix(m, i, 0);
    result += sign * m[i][0] * determinant(sub_m);
  }

  return result;
}

template <> float determinant(const mat<2> &m);

template <int N> mat<N> transpose(const mat<N> &m) {
  mat<N> result;
  for (int i = 0; i < N * N; ++i) {
    int row = i % N, col = i / N;
    result[row][col] = m[col][row];
  }

  return result;
}

template <int N> mat<N> inverse(const mat<N> &m) {
  mat4 result;

  float det = determinant(m);
  if (det == 0)
    return m;

  mat<N> complement_matrix;
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      float sign = powf(-1, static_cast<float>(i + j + 2));
      complement_matrix[i][j] = sign * determinant(create_submatrix(m, i, j));
    }
  }

  result = 1 / det * transpose(complement_matrix);
  return result;
}

}; // namespace Transform

#endif // TRANSFORM_H

