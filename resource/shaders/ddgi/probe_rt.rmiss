#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require
#include "ddgi_common.glsl"

layout(location = 0) rayPayloadInEXT vec4 payload;

void main() {
    vec3 dir = normalize(gl_WorldRayDirectionEXT.xyz);
    vec3 radiance = texture(uSkybox, dir).rgb;
    float distance = 1000.0f;
    payload = vec4(radiance, distance);
}