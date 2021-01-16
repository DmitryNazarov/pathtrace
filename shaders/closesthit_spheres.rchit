#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#include "raycommon.glsl"

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
layout(binding = 5, set = 0) buffer Spheres { Sphere s[]; } spheres;
layout(binding = 6, set = 0) buffer PointLights { PointLight l[]; } pointLights;
layout(binding = 7, set = 0) buffer DirectLights { DirectionLight l[]; } directLights;
layout(binding = 9, set = 0) buffer SphereMaterials { Material m[]; } sphereMaterials;


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
	vec3 intersectionPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	Sphere instance = spheres.s[gl_PrimitiveID];
	vec3 normal = normalize(intersectionPoint - instance.pos);
	Material mat = sphereMaterials.m[gl_PrimitiveID];

	vec4 finalColor = computeShading(intersectionPoint, gl_WorldRayOriginEXT, normal, mat);
	hitValue = finalColor.rgb;

//   // Diffuse
//   vec3  diffuse     = computeDiffuse(mat, L, normal);
//   vec3  specular    = vec3(0);
//   float attenuation = 0.3;

//   // Tracing shadow ray only if the light is visible from the surface
//   if(dot(normal, L) > 0)
//   {
//     float tMin   = 0.001;
//     float tMax   = lightDistance;
//     vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
//     vec3  rayDir = L;
//     uint  flags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT
//                  | gl_RayFlagsSkipClosestHitShaderEXT;
//     shadowed = true;
//     traceRayEXT(topLevelAS,  // acceleration structure
//                 flags,       // rayFlags
//                 0xFF,        // cullMask
//                 0,           // sbtRecordOffset
//                 0,           // sbtRecordStride
//                 1,           // missIndex
//                 origin,      // ray origin
//                 tMin,        // ray min range
//                 rayDir,      // ray direction
//                 tMax,        // ray max range
//                 1            // payload (location = 1)
//     );

//     if(shadowed)
//     {
//       attenuation = 0.3;
//     }
//     else
//     {
//       attenuation = 1;
//       // Specular
//       specular = computeSpecular(mat, gl_WorldRayDirectionEXT, L, normal);
//     }
//   }

//   hitValue = vec3(lightIntensity * attenuation * (diffuse + specular));
}
