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

void traceRay(vec3 origin, vec3 dir)
{
	shadowed = true;
	float tmin = 0.001;
	float tmax = 10000.0;
	traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT,
		0xFF, 1, 0, 1, origin, tmin, dir, tmax, 2);
}

vec4 computeLight(vec3 direction, vec4 lightcolor, vec3 normal,
	vec3 halfvec, vec4 diffuse, vec4 specular, float shininess)
{
	float nDotL = dot(normal, direction);
	vec4 lambert = diffuse * max(nDotL, 0.0f);

	float nDotH = dot(normal, halfvec);
	vec4 phong = specular * pow(max(nDotH, 0.0f), shininess);

	return lightcolor * (lambert + phong);
}

vec4 computeShading(vec3 point, vec3 eye, vec3 normal, Material m)
{
	vec4 finalcolor = m.ambient + m.emission;
	vec3 direction, halfvec;
	vec3 eyedirn = normalize(eye - point);

	//finalcolor = vec4((((normal.x - 0.0f) < 0.01f) ? 1.0f : 0.0f),(((normal.y - 0.0f) < 0.01f) ?  1.0f : 0.0f),(((normal.z - -1.0f) < 0.01f) ? 1.0f : 0.0f), 1.0f);

	for (int i = 0; i < ubo.directLightsNum; ++i)
	{
		direction = normalize(directLights.l[i].dir);
		//direction = normalize(vec3(directLights.l[i].dir.x, -directLights.l[i].dir.y, directLights.l[i].dir.z));

		//compensate_float_rounding_error(shadow_ray, normal);???
		//traceRay(point, direction);
		//if (!shadowed)
		//{
			halfvec = normalize(direction + eyedirn);
			finalcolor += computeLight(direction, directLights.l[i].color, normal, halfvec, m.diffuse,
				m.specular, m.shininess);
		//}
	}

	for (int i = 0; i < ubo.pointLightsNum; ++i)
	{
		vec3 lightdir = pointLights.l[i].pos - point;
		direction = normalize(lightdir);
		//Ray shadow_ray(point, direction);
		//compensate_float_rounding_error(shadow_ray, normal);???
		float dist = length(lightdir);
		// vec3 hit_point;
		// int index = 0;
		bool is_hidden_by_other_obj = false;
		// if (cast_ray(shadow_ray, hit_point, index) && obj_index != index) {
		// 	auto l = length(hit_point - point);
		// 	is_hidden_by_other_obj = length(hit_point - point) < dist;
		// }

		if (!is_hidden_by_other_obj) {
			halfvec = normalize(direction + eyedirn);
			vec4 color = computeLight(direction, pointLights.l[i].color, normal, halfvec, m.diffuse,
				m.specular, m.shininess);
			float a = pointLights.l[i].attenuation.x + pointLights.l[i].attenuation.y * dist +
				pointLights.l[i].attenuation.z * dist * dist;
			finalcolor += color / a;
		}
	}

	if (finalcolor.a > 1.0f)
		finalcolor.a = 1.0f;
	return finalcolor;
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
	//normal = normalize(vec3(ubo.viewInverse * ubo.projInverse * vec4(normal, 0.0)));
	Material mat = triangleMaterials.m[gl_PrimitiveID];
	vec3 intersectionPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	//intersectionPoint = vec3(ubo.viewInverse * ubo.projInverse * vec4(intersectionPoint, 1.0f));
	vec4 finalColor = computeShading(intersectionPoint, gl_WorldRayOriginEXT, normal, mat);
	hitValue = finalColor.rgb;
}
