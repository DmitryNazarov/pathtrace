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

struct RayPayload
{
	vec3 color;
	vec3 intersectionPoint;
	vec3 normal;
	vec3 specular;
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

vec3 compensateFloatRoundingError(vec3 origin, vec3 direction, vec3 normal) {
	if (dot(direction, normal) < 0.0f)
		origin -= 1e-4f * normal;
	else
		origin += 1e-4f * normal;

	return origin;
}