#version 450
layout(location = 0) in vec3 PosView;
layout(location = 0) out float outDepth;

layout(set = 0, binding = 0) uniform VPMatrices {
    mat4 mViewMatrix;
    mat4 mProjectionMatrix;
} vpUBO;


//float getNearFromProj(mat4 P){
//    float m22 = P[2][2];
//    float m32 = P[3][2];
//    return m32 / (m22 - 1.0);
//}


void main() {
    float dist = -PosView.z; 
    outDepth = 0.1 / dist; 
}