#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#include "raycommon.glsl"


layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
layout(location = 1) rayPayloadEXT bool isShadowed;
hitAttributeEXT vec3 attribs;
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0) uniform UBO
{
	mat4 viewInverse;
	mat4 projInverse;
	uint pointLightsNum;
	uint directLightsNum;
	uint quadLightsNum;
	uint lightsamples;
	uint lightstratify;
} ubo;
layout(binding = 3, set = 0) buffer Vertices { Vertex v[]; } vertices;
layout(binding = 4, set = 0) buffer Indices { uint i[]; } indices;
layout(binding = 6, set = 0) buffer PointLights { PointLight l[]; } pointLights;
layout(binding = 7, set = 0) buffer DirectLights { DirectionLight l[]; } directLights;
layout(binding = 8, set = 0) buffer TriangleMaterials { Material m[]; } triangleMaterials;

void traceRay(vec3 origin, vec3 dir, float dist)
{
	uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
	traceRayEXT(topLevelAS,  // acceleration structure
				flags,       // rayFlags
				0xFF,        // cullMask
				0,           // sbtRecordOffset
				0,           // sbtRecordStride
				1,           // missIndex
				origin,      // ray origin
				0.001,       // ray min range
				dir,         // ray direction
				dist,        // ray max range
				1            // payload (location = 1)
	);
}

vec4 computeShading(vec3 point, vec3 eye, vec3 normal, Material m)
{
	vec4 finalcolor = m.ambient + m.emission;
	vec3 direction, halfvec;
	vec3 eyedirn = normalize(eye - point);

	for (int i = 0; i < ubo.directLightsNum; ++i)
	{
		direction = normalize(directLights.l[i].dir);
		isShadowed = true;
		traceRay(point, direction, 10000.0f);
		if (!isShadowed)
		{
			halfvec = normalize(direction + eyedirn);
			finalcolor += computeLight(direction, directLights.l[i].color, normal, halfvec, m.diffuse,
				m.specular, m.shininess);
		}
	}

	for (int i = 0; i < ubo.pointLightsNum; ++i)
	{
		vec3 lightdir = pointLights.l[i].pos - point;
		direction = normalize(lightdir);
		float dist = length(lightdir);

		isShadowed = true;
		if (dot(normal, direction) > 0)
		{
			traceRay(point, direction, dist);
		}

		if (!isShadowed)
		{
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
	Vertex v0 = vertices.v[indices.i[3 * gl_PrimitiveID]];
	Vertex v1 = vertices.v[indices.i[3 * gl_PrimitiveID + 1]];
	Vertex v2 = vertices.v[indices.i[3 * gl_PrimitiveID + 2]];

	// Interpolate normal
	const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);
	Material mat = triangleMaterials.m[gl_PrimitiveID];
	vec3 intersectionPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	vec4 finalColor = computeShading(intersectionPoint, gl_WorldRayOriginEXT, normal, mat);

	rayPayload.color = finalColor.rgb;
	rayPayload.intersectionPoint = intersectionPoint;
	rayPayload.normal = normal;
	rayPayload.specular = mat.specular.rgb;
}
