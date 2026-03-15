layout(set = 0, binding = 0) uniform VPMatrices {
    mat4 mViewMatrix;
    mat4 mProjectionMatrix;
};

layout(set = 0, binding = 1) uniform lightParams{
    vec4 lightDir;
    float intensity;
    float lightRadius;
    uint numFrames;
};

layout(set = 0, binding = 2) uniform taaParams{
    mat4 inverseVP;   
    mat4 prevVP;      
 	vec2 currPixelOffset;
    vec2 prevPixelOffset;        
    vec2 resolution;   
};

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

struct DrawData {
    uint transformId;
    uint materialId;
};

layout(set = 1, binding = 1) readonly buffer Transforms { mat4 worldMatrices[];};
layout(set = 1, binding = 2) readonly buffer DrawDataBuffer { DrawData dd[]; };
layout(set = 1, binding = 3) readonly buffer MaterialParams { Material materials[]; };
layout(set = 1, binding = 4) uniform accelerationStructureEXT uAS;
layout(set = 1, binding = 5) uniform sampler2D Textures[];

struct Submesh {
    uint indexOffset;
    uint vertexOffset;
    uint materialId;
    uint pad0;
};

layout(set = 2, binding = 0) uniform DDGIParams{ 
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

layout(set = 2, binding = 1, rgba16f) uniform image2D uRadiance;
layout(set = 2, binding = 2) readonly buffer PositionBuffer { vec4 positions[]; };
layout(set = 2, binding = 3) readonly buffer UvBuffer       { vec2 uvs[]; };
layout(set = 2, binding = 4) readonly buffer NormalBuffer   { vec4 normals[];   };
layout(set = 2, binding = 5) readonly buffer IndexBuffer    { uint indices[]; };
layout(set = 2, binding = 6) uniform samplerCube uSkybox;
layout(set = 2, binding = 7) readonly buffer SubMeshBuffer { Submesh submeshes[]; };
layout(set = 2, binding = 8) uniform sampler2D uIrradiance;
layout(set = 2, binding = 9) uniform sampler2D uVisibility;
layout(set = 2, binding = 10)  buffer ProbeStatus { uint probeStatus[];};
layout(set = 2, binding = 11) uniform sampler2D uOffset;

#define PI 3.1415926538