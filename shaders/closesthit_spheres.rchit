#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec3 attribs;
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 5, set = 0) buffer Spheres { Sphere s[]; } spheres;
layout(binding = 6, set = 0) buffer PointLights { PointLight l[]; } pointLights;
layout(binding = 7, set = 0) buffer DirectLights { DirectionLight l[]; } directLights;
layout(binding = 9, set = 0) buffer SphereMaterials { Material m[]; } sphereMaterials;


void main()
{
  vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

  Sphere instance = spheres.s[gl_PrimitiveID];

  // Computing the normal at hit position
  vec3 normal = normalize(worldPos - instance.pos);

//   // Vector toward the light
//   vec3  L;
//   float lightIntensity = pushC.lightIntensity;
//   float lightDistance  = 100000.0;
//   // Point light
//   if(pushC.lightType == 0)
//   {
//     vec3 lDir      = pushC.lightPosition - worldPos;
//     lightDistance  = length(lDir);
//     lightIntensity = pushC.lightIntensity / (lightDistance * lightDistance);
//     L              = normalize(lDir);
//   }
//   else  // Directional light
//   {
//     L = normalize(pushC.lightPosition - vec3(0));
//   }

//   Material mat = triangleMaterials.m[gl_PrimitiveID];

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
	hitValue = vec3(0.5f);
}