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
layout(binding = 10, set = 0) buffer QuadLights { QuadLight q[]; } quadLights;


uint rngState = gl_LaunchSizeEXT.x * gl_LaunchIDEXT.y + gl_LaunchIDEXT.x;  // Initial seed

void traceShadowRay(vec3 origin, vec3 dir, float dist)
{
	uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
	traceRayEXT(topLevelAS,  // acceleration structure
				flags,       // rayFlags
				0xFF,        // cullMask
				0,           // sbtRecordOffset
				0,           // sbtRecordStride
				1,           // missIndex
				origin,      // ray origin
				EPS,         // ray min range
				dir,         // ray direction
				dist - EPS,  // ray max range
				1            // payload (location = 1)
	);
}

vec3 getLightPos(QuadLight q, int s, int gridWidth)
{
	//uint seed = Tea(Tea(gl_LaunchSizeEXT.x, gl_LaunchIDEXT.y), gridWidth * gridWidth);
	//float u1 = Randf01(seed);
	//float u2 = Randf01(seed);
	float u1 = stepAndOutputRNGFloat(rngState);
	float u2 = stepAndOutputRNGFloat(rngState);
	//if (abs(u1 - 1.f) < EPS) u1 = 1.0f - 2 * EPS;
	//if (abs(u2 - 1.f) < EPS) u2 = 1.0f - 2 * EPS;
	if (ubo.lightstratify != 0)
	{
		int j = s / gridWidth;
		int k = s % gridWidth;
		u1 = (u1 + k) / gridWidth;
		u2 = (u2 + j) / gridWidth;
	}
	return q.pos + u1 * q.abSide + u2 * q.acSide;
}

vec4 computeShading(vec3 point, vec3 eyedir, vec3 normal, Material m)
{
	//return vec4(1.f);

	vec4 finalcolor = m.ambient + m.emission;
	vec3 direction, halfvec;

	for (int i = 0; i < ubo.directLightsNum; ++i)
	{
		direction = normalize(directLights.l[i].dir);
		isShadowed = true;
		traceShadowRay(point, direction, 10000.0f);
		if (!isShadowed)
		{
			halfvec = normalize(direction + eyedir);
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
			traceShadowRay(point, direction, dist);
		}

		if (!isShadowed)
		{
			halfvec = normalize(direction + eyedir);
			vec4 color = computeLight(direction, pointLights.l[i].color, normal, halfvec, m.diffuse,
				m.specular, m.shininess);
			float a = pointLights.l[i].attenuation.x + pointLights.l[i].attenuation.y * dist +
				pointLights.l[i].attenuation.z * dist * dist;
			finalcolor += color / a;
		}
	}

	if (m.emission.xyz == vec3(0))
	{
		for (int i = 0; i < ubo.quadLightsNum; ++i)
		{
			vec4 irradiance = vec4(0.0f);
			float cosOfAngle = dot(normalize(quadLights.q[i].abSide), normalize(quadLights.q[i].acSide));
			float sinOfAngle = sqrt(1 - cosOfAngle * cosOfAngle);
			float area = length(quadLights.q[i].abSide) * length(quadLights.q[i].acSide) * sinOfAngle;
			int stratifiedGridWidth = int(sqrt(ubo.lightsamples));
			for (int s = 0; s < ubo.lightsamples; ++s)
			{
				vec3 lightpos = getLightPos(quadLights.q[i], s, stratifiedGridWidth);
				vec3 lightdir = lightpos - point;
				direction = normalize(lightdir);
				float dist = length(lightdir);
				isShadowed = true;
				if (dot(normal, direction) > 0)
				{
					traceShadowRay(point, direction, dist);
				}
				if (isShadowed)
				{
					continue;
				}

				vec4 reflectance = computeLight(direction, eyedir, normal, m.diffuse, m.specular, m.shininess);
				float cosOmegaO = dot(quadLights.q[i].normal, direction);
				float cosOmegaI = dot(normal, direction);
				float geom = max(cosOmegaI, 0.0f) * max(cosOmegaO, 0.0f) / (dist * dist);
				irradiance += reflectance * geom;
				// if (gl_LaunchIDEXT.x == 292 && gl_LaunchIDEXT.y == 366)
				// {
				// 	//if (abs(color.r) > EPS && abs(color.g) > EPS && abs(color.b) > EPS)
				// 	if (cosOmegaO < EPS)
				// 		color = vec4(1, 0, 0, 0);
				// 	//else
				// 		//finalcolor = vec4(0, 1, 0, 0);
				// }

				// if (cosOmegaO < EPS)
				// 	irradiance = vec4(1, 0, 0, 0);
			}
			finalcolor += quadLights.q[i].color * irradiance * area / ubo.lightsamples;
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
	vec4 finalColor = computeShading(intersectionPoint, -gl_WorldRayDirectionEXT, normal, mat);

	rayPayload.color = finalColor.rgb;
	rayPayload.intersectionPoint = intersectionPoint;
	rayPayload.normal = normal;
	rayPayload.specular = mat.specular.rgb;
}
