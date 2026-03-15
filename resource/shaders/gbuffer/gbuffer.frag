#version 450
#extension GL_EXT_nonuniform_qualifier : enable

precision highp float;
precision mediump int;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in mat3 tbn;
layout(location = 6) in flat uint matID;
layout(location = 7) in flat uint meshID;
layout(location = 8) in vec4 worldPos;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outMR;

struct Material {
    vec4 baseColorFactor;
    vec4 emissiveFactor;

    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    uint alphaMode;

    uint baseColorTexture;
    uint metallicRoughnessTexture;
    uint normalTexture;
    uint emissiveTexture;
};

layout(set = 1, binding = 3) readonly buffer MaterialParams { Material materials[]; };
layout(set = 1, binding = 5) uniform sampler2D Textures[];

layout(push_constant) uniform PushConstants {
    vec4 lightDir;
    vec4 cameraPos;
} pc;

void main(){
    // albedo
    vec4 baseColor = materials[matID].baseColorFactor;
    baseColor *= texture(Textures[materials[matID].baseColorTexture], fragUV);
    if (materials[matID].alphaMode == 1u && baseColor.a < materials[matID].alphaCutoff) {
        discard;
    }
    outColor = baseColor;
    
    // normal
    vec3 normal = normalize(fragNormal);
    vec3 sampledNormal = texture(Textures[materials[matID].normalTexture], fragUV).xyz;
    sampledNormal = sampledNormal * 2.0 - 1.0;
    normal = normalize(tbn * sampledNormal);
    outNormal.rgb = normal;
    outNormal.a = float(meshID) / 255.0;

    // metalic and roughness
    float metallic = materials[matID].metallicFactor;
    float roughness = materials[matID].roughnessFactor;
    if(materials[matID].metallicRoughnessTexture > 0){
        vec4 rm = texture(Textures[materials[matID].metallicRoughnessTexture], fragUV);
        roughness = rm.g;
        metallic = rm.b;
    }

    outMR = vec4(0.0, roughness, metallic, 1.0);

}




