#version 450


layout(set = 0, binding = 1) uniform light{
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

layout (set = 1, binding = 0) uniform sampler2D colorTex;
layout (set = 1, binding = 1) uniform sampler2D normalTex;
layout (set = 1, binding = 2) uniform sampler2D mrTex;
layout (set = 1, binding = 3) uniform sampler2D depthTex;
layout (set = 1, binding = 4) uniform samplerCube irradianceMap;
layout (set = 1, binding = 5) uniform sampler2D visibilityTex;
layout (set = 1, binding = 7) uniform sampler2D uIndirect;
layout (set = 1, binding = 8) uniform usampler2D uAO;

layout (location = 0) in vec2 vUV;
layout (location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec4 cameraPos;
    float near;
	float far;
    uint enableDiffuseIBL;
    float diffuseIBLIntensity;
    uint enableGTAO;
    uint enableDDGI;
    float ddgiIntensity;
    uint  viewMode;  
} pc;

#define PI 3.1415926538


vec3 uvToNDC(vec2 uv, float depth){
    return vec3( uv.x * 2.0f - 1.0f,  uv.y * 2.0f - 1.0f, depth);
}

vec3 uvToWorldPos(vec2 uv, float depth, mat4 inverseVP){
    vec4 constructed = vec4(uvToNDC(uv, depth), 1.0f);
    vec4 unprojected = inverseVP * constructed;
    return unprojected.xyz / unprojected.w;
}

float distributionGGX(vec3 N, vec3 H, float roughness){
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float geometrySchlickGGX(float NdotV, float roughness){
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness){
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float linearizeDepth(float d, float near, float far){
    return (near * far) / (far - d * (far - near));
}

void main() {
    vec4 baseColor = texture(colorTex, vUV);
    vec3 normal = texture(normalTex, vUV).rgb;
    vec3 rm = texture(mrTex, vUV).rgb;
    float depth = texture(depthTex, vUV).r;
    vec3 pos = uvToWorldPos(vUV, depth, inverseVP);
    
    vec3 V = normalize( pc.cameraPos.xyz - pos );
    vec3 L = normalize( -lightDir.xyz );
    vec3 N = normal;
    vec3 H = normalize( L + V );

    float roughness = rm.g;
    float metallic = rm.b;
    float alpha = pow(roughness, 2.0);
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, baseColor.rgb, metallic);

    // D,G,F
    float NDF = distributionGGX(N, H, roughness);   
    float G   = geometrySmith(N, V, L, roughness);      
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    // BDRF 
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    vec3 numerator    = NDF * G * F; 
    float denominator = max(4.0 * NdotV * NdotL, 1e-6);
    vec3 specular = numerator / denominator;
    vec3 kS = F;
    
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    // 1 direct diffuse and specular
    vec3 lightColor = vec3(1.0);
    vec3 Lo = (kD * baseColor.rgb / PI + specular) *  NdotL * intensity;
    float visibility = texture(visibilityTex, vUV).r;
    vec3 direct = Lo * visibility;

    // 2 ibl
    float enableDiffuseIbl = float(pc.enableDiffuseIBL);
    vec3 Eibl = texture(irradianceMap, normal).rgb;
    vec3 diffuseIbl = enableDiffuseIbl * (kD * baseColor.rgb) * Eibl * pc.diffuseIBLIntensity;
    
    // 3 gi
    float enableDDGI = float(pc.enableDDGI);
    vec3 Eindirect = texture(uIndirect, vUV).rgb;
    vec3 gi = enableDDGI * (kD * baseColor.rgb) * Eindirect * pc.ddgiIntensity;

    // 4 ao
    float enableGTAO = float(pc.enableGTAO);
    uint aoU8 = texture(uAO, vUV).r; 
    float aoVis = float(aoU8) / 255.0; 
    gi *= mix(1.0, aoVis, enableGTAO);

    // 4 final
    vec3 finalColor = direct + diffuseIbl + gi;

    if (pc.viewMode == 1u) {
        finalColor = (normal + 1.0) * 0.5;
    }
    else if (pc.viewMode == 2u) {
        float z = linearizeDepth(depth, pc.near, pc.far);
        finalColor = vec3(z / pc.far);
    }
    else if (pc.viewMode == 3u) {
        finalColor = vec3(0.0, rm.g, 0.0);
    }
    else if (pc.viewMode == 4u) {
        finalColor = vec3(0.0, 0.0, rm.b);
    }
    else if (pc.viewMode == 5u) {
        finalColor = Eibl;
    }
    else if (pc.viewMode == 6u) {
        finalColor = vec3(visibility);
    }
    else if (pc.viewMode == 7u) {
        finalColor = Eindirect;
    }
    else if (pc.viewMode == 8u) {
        finalColor = vec3(aoVis);
    }

    outColor = vec4(finalColor, 1.0);
}
    