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

struct QuadLight
{
	vec3 pos;
	vec3 abSide;
	vec3 acSide;
	vec3 normal;
	vec4 color;
};

const float PI = 3.1415926535897932384626433832795;
const float EPS = 0.001;

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

// float PHI = 1.61803398874989484820459;  // Golden Ratio
// float gold_noise(in vec2 xy, in float seed)
// {
// 	return fract(tan(distance(xy * PHI, xy) * seed) * xy.x);
// }

// float rand(vec2 p)
// {
// 	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
// }

// float rand2(vec2 p)
// {
// 	return fract(cos(dot(p, vec2(23.14069263277926, 2.665144142690225))) * 12345.6789);
// }

// Steps the RNG and returns a floating-point value between 0 and 1 inclusive.
float stepAndOutputRNGFloat(inout uint rngState)
{
	// Condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0,1].
	rngState  = rngState * 747796405 + 1;
	uint word = ((rngState >> ((rngState >> 28) + 4)) ^ rngState) * 277803737;
	word      = (word >> 22) ^ word;
	return float(word) / 4294967295.0f;
}

vec4 computeLight(vec3 direction, vec3 eyedir, vec3 normal, vec4 diffuse, vec4 specular, float shininess)
{
	vec4 lambert = diffuse / PI;
	vec4 phong = specular * (shininess + 2) / (2 * PI) * pow(max(dot(reflect(-eyedir, normal), direction), 0.0f), shininess);
	return lambert + phong;
}