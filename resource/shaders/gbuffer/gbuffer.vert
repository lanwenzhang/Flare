#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out mat3 tbn;
layout(location = 6) out flat uint matID;
layout(location = 7) out flat uint meshID;
layout(location = 8) out vec4 worldPos;

struct DrawData {
    uint transformId;
    uint materialId;
};

layout(set = 0, binding = 0) uniform VPMatrices {
    mat4 mViewMatrix;
    mat4 mProjectionMatrix;
} vpUBO;

layout(set = 1, binding = 1) readonly buffer Transforms { mat4 worldMatrices[];};
layout(set = 1, binding = 2) readonly buffer DrawDataBuffer { DrawData dd[]; };

void main() {
    uint transformIndex = dd[gl_BaseInstance].transformId;
    mat4 model = worldMatrices[transformIndex];
    worldPos = model * vec4(inPosition, 1.0);
    
    fragPos = worldPos.xyz;
    fragUV = inUV;
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    fragNormal = normalMatrix * inNormal;
//
//    vec3 fragTangent = normalize(mat3(model) * inTangent.xyz);
//    vec3 fragBitangent = normalize(cross(fragNormal, fragTangent));
//    tbn = mat3(fragTangent, fragBitangent, fragNormal);
    vec3 N = normalize(fragNormal);
    vec3 T = normalize(normalMatrix * inTangent.xyz);
    T = normalize(T - N * dot(N, T));
    vec3 B = normalize(cross(N, T));
    tbn = mat3(T, B, N);

    meshID = gl_BaseInstance;
    matID = dd[gl_BaseInstance].materialId;

    gl_Position = vpUBO.mProjectionMatrix * vpUBO.mViewMatrix * worldPos;
}

