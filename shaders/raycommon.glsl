struct Vertex
{
  vec3 pos;
  vec3 normal;
 };

struct Sphere
{
  vec3 pos;
  float r;
  mat4 transform, invertedTransform;
};

struct PointLight
{
  vec3 pos;
  vec4 color;
  vec3 attenuation;
};

struct DirectionLight
{
  vec3 dir;
  vec4 color;
};

struct Material
{
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  vec4 emission;
  float shininess;
};