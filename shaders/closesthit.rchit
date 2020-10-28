#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec3 attribs;
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0) uniform UBO
{
	mat4 viewInverse;
	mat4 projInverse;
	uint pointLightsNum;
	uint directLightsNum;
} ubo;

struct Vertex
{
  vec3 pos;
  vec3 normal;
 };
layout(binding = 3, set = 0) buffer Vertices { Vertex v[]; } vertices;
layout(binding = 4, set = 0) buffer Indices { uint i[]; } indices;

struct PointLight
{
  vec3 pos;
  vec4 color;
  vec3 attenuation;
};
layout(binding = 5, set = 0) buffer PointLights { PointLight l[]; } pointLights;

struct DirectionLight
{
  vec3 dir;
  vec4 color;
};
layout(binding = 6, set = 0) buffer DirectLights { DirectionLight l[]; } directLights;

struct Material
{
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  vec4 emission;
  float shininess;
};
layout(binding = 7, set = 0) buffer TriangleMaterials { Material m[]; } triangleMaterials;
layout(binding = 8, set = 0) buffer SphereMaterials { Material m[]; } sphereMaterials;

void compute_shading()
{

}

void main()
{
	ivec3 index = ivec3(indices.i[3 * gl_PrimitiveID], indices.i[3 * gl_PrimitiveID + 1], indices.i[3 * gl_PrimitiveID + 2]);

	Vertex v0 = Vertex(vertices.v[index.x].pos, vertices.v[index.x].normal);
	Vertex v1 = Vertex(vertices.v[index.y].pos, vertices.v[index.y].normal);
	Vertex v2 = Vertex(vertices.v[index.z].pos, vertices.v[index.z].normal);

	// Interpolate normal
	const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);

	Material mat = triangleMaterials.m[gl_PrimitiveID];
	compute_shading();

	// Basic lighting
	vec3 lightVector = normalize(pointLights.l[0].pos);
	float dot_product = max(dot(lightVector, normal), 0.2);
	hitValue = mat.diffuse.rgb * dot_product;

	// Shadow casting
	float tmin = 0.001;
	float tmax = 10000.0;
	vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	shadowed = true;
	// Trace shadow ray and offset indices to match shadow hit/miss shader group indices
	traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, origin, tmin, lightVector, tmax, 2);
	if (shadowed) {
		hitValue *= 0.3;
	}
}
