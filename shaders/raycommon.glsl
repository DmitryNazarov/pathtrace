struct Ray
{
	vec3 origin;
	vec3 direction;
};

struct Vertex
{
	vec3 pos;
	vec3 normal;
 };

struct Sphere
{
	vec3 pos;
	float radius;
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

vec4 computeLight(vec3 direction, vec4 lightcolor, vec3 normal,
	vec3 halfvec, vec4 diffuse, vec4 specular, float shininess)
{
	float nDotL = dot(normal, direction);
	vec4 lambert = diffuse * max(nDotL, 0.0f);

	float nDotH = dot(normal, halfvec);
	vec4 phong = specular * pow(max(nDotH, 0.0f), shininess);

	return lightcolor * (lambert + phong);
}

//Lambertian
vec4 computeLightDiffuse(vec3 direction, vec3 normal, vec4 diffuse)
{
	float nDotL = dot(normal, direction);
	return diffuse * max(nDotL, 0.0f);
}

//Phong
vec4 computeLightSpecular(vec3 direction, vec3 normal, vec3 halfvec,
	vec4 specular, float shininess)
{
	float nDotH = dot(normal, halfvec);
	return specular * pow(max(nDotH, 0.0f), shininess);
}