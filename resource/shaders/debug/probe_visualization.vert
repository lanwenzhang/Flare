#version 460

layout(location = 0) in vec3 position;
layout(location = 0) flat out int  probeIndex;
layout(location = 1) out vec4      normalEdgeFactor;

layout(set = 0, binding = 0) uniform VPMatrices {
    mat4 mViewMatrix;
    mat4 mProjectionMatrix;
}vpUBO;

layout(set = 1, binding = 0) uniform DDGIParams{ 
    vec4  probeGridPosition;
    vec4  probeSpacing; 
    vec4  reciprocalProbeSpacing;
    ivec4 probeCounts;

    int   irradianceWidth;
    int   irradianceHeight;
    int   irradianceSideLength;
    int   probeRays;

    float selfShadowBias;
    float hysteresis;
    float infiniteBounceMultiplier;
    float maxOffset;

    uint enableInfiniteBounce;
    uint enableBackfaceBlending;
    uint enableSmoothBackface;
    uint enableProbeOffset;

    mat4  randomRotation;
};

layout(set = 2, binding = 0) uniform sampler2D irradiance;
layout(set = 2, binding = 1) uniform sampler2D uOffset;

layout(push_constant) uniform PushConstants {
    vec4 cameraPos;
} pc;

ivec3 probeToGrid( int probeIndex ) {
    const int probeX = probeIndex % probeCounts.x;
    const int probeCountsXY = probeCounts.x * probeCounts.y;
    const int probeY = (probeIndex % probeCountsXY) / probeCounts.x;
    const int probeZ = probeIndex / probeCountsXY;
    return ivec3( probeX, probeY, probeZ );
}

vec3 gridToWorldNoOffsets( ivec3 gridIndex ) {
    return gridIndex * probeSpacing.xyz + probeGridPosition.xyz;
}

vec3 gridToWorld( ivec3 gridIndex, int probeIndex ) {
  const int probeCountsXY = probeCounts.x * probeCounts.y;
  ivec2 probeOffsetSamplingUV = ivec2(probeIndex % probeCountsXY, probeIndex / probeCountsXY);
  vec3 probeOffset = enableProbeOffset == 1u ? texelFetch(uOffset, probeOffsetSamplingUV, 0).rgb : vec3(0);
  return gridToWorldNoOffsets( gridIndex ) + probeOffset;
}

void main(){
    probeIndex = gl_InstanceIndex;
    ivec3 probeGridIndices = probeToGrid(probeIndex);
    vec3  probePosition   = gridToWorld(probeGridIndices, probeIndex);

    gl_Position = vpUBO.mProjectionMatrix * vpUBO.mViewMatrix * vec4(position * probeSpacing.w + probePosition, 1.0);
    normalEdgeFactor.xyz = normalize(position);
    normalEdgeFactor.w = abs(dot(normalEdgeFactor.xyz, normalize(probePosition - pc.cameraPos.xyz)));
}
