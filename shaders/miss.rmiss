#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

void main()
{
	rayPayload.color = vec3(0.0f);
	rayPayload.intersectionPoint = vec3(-1.0f);
	rayPayload.normal = vec3(0.0f);
	rayPayload.specular = vec3(0.0f);
}