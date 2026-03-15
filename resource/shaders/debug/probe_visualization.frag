#version 460
layout(location = 0) flat in int probeIndex;
layout(location = 1) in vec4     normalEdgeFactor;
layout(location = 0) out vec4 color;

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

vec2 signNotZero2(vec2 v) {
    return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

vec2 octEncode(in vec3 v){
    float l1Norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2  result = v.xy * (1.0 / l1Norm);
    if (v.z < 0.0) {
        result = (1.0 - abs(result.yx)) * signNotZero2(result.xy);
    }
    return result;
}


vec2 getProbeUv(vec3 direction, int probeIndex, int width, int height, int sideLength) {

    vec2 octahedralCoordinates = octEncode(normalize(direction));

    float probeWithBorderSide = float(sideLength) + 2.0;
    int   probesPerRow        = width / int(probeWithBorderSide);
    ivec2 probeIndices = ivec2(probeIndex % probesPerRow, probeIndex / probesPerRow);

    vec2 atlasTexels = vec2(probeIndices.x * probeWithBorderSide, probeIndices.y * probeWithBorderSide);

    atlasTexels += vec2(1.0);
    atlasTexels += vec2(sideLength * 0.5);

    atlasTexels += octahedralCoordinates * (sideLength * 0.5);
    vec2 uv = atlasTexels / vec2(float(width), float(height));

    return uv;
}


void main(){
    vec2 uv = getProbeUv(normalEdgeFactor.xyz, probeIndex, irradianceWidth, irradianceHeight, irradianceSideLength);
    vec3 irradiance = texture(irradiance, uv, 0).rgb;
    if (normalEdgeFactor.w < 0.55) {
        irradiance *= 0.2;
    }
    color = vec4(irradiance, 1.0);
}