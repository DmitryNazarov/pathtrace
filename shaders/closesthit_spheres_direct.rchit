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
layout(binding = 5, set = 0) buffer Spheres { Sphere s[]; } spheres;
layout(binding = 6, set = 0) buffer PointLights { PointLight l[]; } pointLights;
layout(binding = 7, set = 0) buffer DirectLights { DirectionLight l[]; } directLights;
layout(binding = 9, set = 0) buffer SphereMaterials { Material m[]; } sphereMaterials;
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
	float u1 = stepAndOutputRNGFloat(rngState);
	float u2 = stepAndOutputRNGFloat(rngState);
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
			vec4 color = vec4(0.0f);
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

				vec4 F = computeLight(direction, eyedir, normal, m.diffuse, m.specular, m.shininess);
				float cosOmegaO = dot(quadLights.q[i].normal, direction);
				// if (cosOmegaO < 0)
				// 	cosOmegaO = dot(-quadLights.q[i].normal, direction);
				float cosOmegaI = dot(normal, direction);
				float geom = max(cosOmegaI, 0.0f) * max(cosOmegaO, 0.0f) / (dist * dist);
				color += F * geom;
			}
			finalcolor += quadLights.q[i].color * color * area / ubo.lightsamples;
		}
	}

	if (finalcolor.a > 1.0f)
		finalcolor.a = 1.0f;
	return finalcolor;
}

void main()
{
	Sphere s = spheres.s[gl_PrimitiveID];

	vec3 intersectionPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	vec3 intersectionPointTransf = (s.invertedTransform * vec4(intersectionPoint, 1.0f)).xyz;
	vec3 normal = normalize(mat3(transpose(s.invertedTransform)) * vec3(intersectionPointTransf - s.pos));

	Material mat = sphereMaterials.m[gl_PrimitiveID];

	vec4 finalColor = computeShading(intersectionPoint, -gl_WorldRayDirectionEXT, normal, mat);

	rayPayload.color = finalColor.rgb;
	rayPayload.intersectionPoint = intersectionPoint;
	rayPayload.normal = normal;
	rayPayload.specular = mat.specular.rgb;
}
