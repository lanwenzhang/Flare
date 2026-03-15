#version 450

layout(location = 0) in vec3 vDirection;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 6) uniform samplerCube uCubemap;

void main() {
    
     outColor = texture(uCubemap, normalize(vDirection));
//    outColor = vec4(1.0, 0.0, 1.0, 1.0);

}