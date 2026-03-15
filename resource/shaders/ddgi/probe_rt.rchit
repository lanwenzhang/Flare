#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier  : enable
#include "ddgi_common.glsl"

layout(location = 0) rayPayloadInEXT vec4 payload;
hitAttributeEXT vec2 barycentricWeights;

layout(push_constant) uniform PushConstants {
    vec4 cameraPos;
} pc;

vec2 pixelToUV(ivec2 pixel, vec2 resolution){
    return (vec2(pixel) + vec2(0.5)) / vec2(resolution);
}

vec3 uvToNDC(vec2 uv, float depth){
    return vec3(uv * 2.0 - 1.0, depth);
}

vec3 uvToWorldPos(vec2 uv, float depth, mat4 invVP){
    vec4 constructed = vec4(uvToNDC(uv, depth), 1.0);
    vec4 unprojected = invVP * constructed;
    return unprojected.xyz / unprojected.w;
}

ivec3 worldToGrid(vec3 worldPos){
    return clamp(ivec3((worldPos - probeGridPosition.xyz) * reciprocalProbeSpacing.xyz), ivec3(0), probeCounts.xyz - ivec3(1));
}

vec3 gridToWorldNoOffsets( ivec3 gridIndex ) {
    return gridIndex * probeSpacing.xyz + probeGridPosition.xyz;
}

vec3 gridToWorld(ivec3 gridIndex, int probeIndex){
    const int probeCountsXY = probeCounts.x * probeCounts.y;
    ivec2 probeOffsetSamplingUV = ivec2(probeIndex % probeCountsXY, probeIndex / probeCountsXY);
    vec3 probeOffset = enableProbeOffset == 1u ? texelFetch(uOffset, probeOffsetSamplingUV, 0).rgb : vec3(0);
    return gridToWorldNoOffsets( gridIndex ) + probeOffset;
}

int gridToIndex(ivec3 probeGrid){
     return int(probeGrid.x + probeGrid.y * probeCounts.x + probeGrid.z * probeCounts.x * probeCounts.y);
}

vec2 signNotZero2(vec2 v) {
    return vec2(
        v.x >= 0.0 ? 1.0 : -1.0,
        v.y >= 0.0 ? 1.0 : -1.0
    );
}

vec2 octEncode(in vec3 v){
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 result = v.xy * (1.0 / l1norm);
    if(v.z < 0.0){
        result = (1.0 - abs(result.yx)) * signNotZero2(result.xy);
    }
    return result;
}

vec2 getProbeUV(vec3 dir, int probeIndex, int width, int height, int sideLength){
    vec2 octahedralCoord = octEncode(dir);
    float probeWithBorder = float(irradianceSideLength) + 2.0f;
    int probePerRow = (width) / int(probeWithBorder);
    ivec2 probeIndexAltas = ivec2((probeIndex % probePerRow), (probeIndex / probePerRow));

    vec2 altasTexel = vec2(probeIndexAltas.x * probeWithBorder, probeIndexAltas.y * probeWithBorder);
    altasTexel += vec2(1.0f);
    altasTexel += vec2(irradianceSideLength * 0.5f);
    altasTexel += octahedralCoord * (irradianceSideLength * 0.5f);
    vec2 uv = altasTexel / vec2(float(width), float(height));
    return uv;
}

vec3 sampleIrradiance(vec3 worldPos, vec3 normal, vec3 camPos){
    vec3 Wo = normalize(camPos - worldPos);

    // 1 calculate the biased point position
    float minProbeDistance = 1.0f;
    vec3 biasVector = (normal * 0.2f + Wo * 0.8f) * (0.75f * minProbeDistance) * selfShadowBias.x;
    vec3 biasedWorldPos = worldPos + biasVector;

    ivec3 baseGrid = worldToGrid(biasedWorldPos);
    vec3 baseProbePos= gridToWorldNoOffsets(baseGrid);
    vec3 alpha = clamp((biasedWorldPos - baseProbePos), vec3(0.0f), vec3(1.0f));

    vec3 sumIrradiance = vec3(0.0f);
    float sumWeight = 0.0f;
    for(int i = 0; i < 8; ++i){
        // 2 calculate probe position
        ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);
        ivec3 probeGrid = clamp(baseGrid + offset, ivec3(0), probeCounts.xyz - ivec3(1));
        int probeIndex = gridToIndex(probeGrid);
        vec3 probePos = gridToWorld(probeGrid, probeIndex);
        
        vec3 trilinear = mix(1.0 - alpha, alpha, offset);
        float weight = 1.0;

        if(enableSmoothBackface != 0u){
            vec3 dirToProbe = normalize(probePos - worldPos);
            float dirDotN = (dot(dirToProbe, normal) + 1.0) * 0.5f;
            weight = (dirDotN * dirDotN) + 0.2;
        }
        // calculate the biased direction
        vec3 BiasedDir = biasedWorldPos - probePos;
        float distanceToBiased = length(BiasedDir);
        BiasedDir *= 1.0 / distanceToBiased;

        // 2 visibility weight
        vec2 visUV = getProbeUV(BiasedDir, probeIndex, irradianceWidth, irradianceHeight, irradianceSideLength);
        vec2 visibility = textureLod(uVisibility, visUV, 0).rg;
        float meanDis = visibility.x;
        float chebyshevWeight = 1.0f;
        // 2.1 in shadow
        if(distanceToBiased > meanDis){
            float variance = abs((visibility.x * visibility.x) - visibility.y);
            float disDiff = distanceToBiased - meanDis;
            chebyshevWeight = variance / (variance + (disDiff * disDiff));
            chebyshevWeight = max((chebyshevWeight * chebyshevWeight * chebyshevWeight), 0.0f);
        }
        chebyshevWeight = max(0.0, chebyshevWeight);
        weight *= chebyshevWeight;
        
        // 2.2 avoid zero
        weight = max(0.000001, weight);

        // 2.3 crush tiny weights 
        float crushThreshold = 0.2f;
        if(weight < crushThreshold){
            weight *= (weight * weight) * (1.0f / (crushThreshold * crushThreshold));
        }
        // 3 irradiance
        vec2 irrUV = getProbeUV(normal, probeIndex, irradianceWidth, irradianceHeight, irradianceSideLength);
        vec3 probeIrradiance = textureLod(uIrradiance, irrUV, 0).rgb;
        probeIrradiance = sqrt(probeIrradiance);

        // 4 trilinear weight
        weight *= trilinear.x * trilinear.y * trilinear.z + 0.001f;
        sumIrradiance += weight * probeIrradiance;
        sumWeight += weight;
    }
    vec3 netIrradiance = sumIrradiance / sumWeight;
    netIrradiance = netIrradiance * netIrradiance;
    vec3 irradiance = 0.5 * PI * netIrradiance * 0.95f;
    return irradiance;
}


void main() {
    vec3 radiance = vec3(0.0);
    float distance = 0.0;

    // 1 backface check
    if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT) {
        distance = gl_RayTminEXT + gl_HitTEXT;
        payload = vec4(0.0, 0.0, 0.0, -0.2 * distance);
        return;
    }

    uint submeshId = gl_InstanceCustomIndexEXT;
    Submesh sm = submeshes[submeshId];
    // 2 barycentric calculation
    // 2.1 indices
    uint base = sm.indexOffset + uint(gl_PrimitiveID) * 3u;
    uint i0 = indices[base + 0u] + sm.vertexOffset;
    uint i1 = indices[base + 1u] + sm.vertexOffset;
    uint i2 = indices[base + 2u] + sm.vertexOffset;

    // 2.2 vertex data 
    vec3 p0 = positions[i0].xyz;
    vec3 p1 = positions[i1].xyz;
    vec3 p2 = positions[i2].xyz;

    vec3 n0 = normals[i0].xyz;
    vec3 n1 = normals[i1].xyz;
    vec3 n2 = normals[i2].xyz;

    vec2 uv0 = uvs[i0];
    vec2 uv1 = uvs[i1];
    vec2 uv2 = uvs[i2];

    // 2.3 barycentric
    float b = barycentricWeights.x;
    float c = barycentricWeights.y;
    float a = 1.0 - b - c;

    vec3 localPos = a * p0 + b * p1 + c * p2;
    vec3 localNormal = normalize(a * n0 + b * n1 + c * n2);
    vec2 uv = a * uv0 + b * uv1 + c * uv2;

    // 3 transform to world
    vec3 worldPos = (gl_ObjectToWorldEXT * vec4(localPos, 1.0)).xyz;
    mat3 normalMatrix = transpose(inverse(mat3(gl_ObjectToWorldEXT)));
    vec3 normal = normalize(normalMatrix * localNormal);

    // 4 lighting calculation
    // 4.1 get albedo
    Material mat = materials[sm.materialId];
    vec3 albedo = mat.baseColorFactor.rgb;
    if (mat.baseColorTexture != 0xFFFFFFFFu) {
        albedo *= texture(Textures[nonuniformEXT(mat.baseColorTexture)], uv).rgb;
    }

    // 4.2 lambert diffuse
    vec3 l = normalize(-lightDir.xyz);
    float nDotL = clamp(dot(normal, l), 0.0, 1.0);

    radiance = albedo * (intensity * nDotL);

    if(enableInfiniteBounce != 0u){
        radiance += albedo * sampleIrradiance(worldPos, normal, pc.cameraPos.xyz) * infiniteBounceMultiplier;
    }
    distance = gl_RayTminEXT + gl_HitTEXT;
    payload = vec4(radiance, distance);
}