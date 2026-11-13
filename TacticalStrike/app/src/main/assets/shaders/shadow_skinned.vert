#version 300 es
precision highp float;

layout(location = 0) in vec3 aPos;
layout(location = 4) in uvec4 aJoints;
layout(location = 5) in vec4 aWeights;

uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;
uniform mat4 uBoneMatrices[32];

void main() {
    float wSum = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
    mat4 skinMat = mat4(1.0);
    if (wSum > 0.001) {
        skinMat = uBoneMatrices[min(aJoints.x, 31u)] * aWeights.x +
                  uBoneMatrices[min(aJoints.y, 31u)] * aWeights.y +
                  uBoneMatrices[min(aJoints.z, 31u)] * aWeights.z +
                  uBoneMatrices[min(aJoints.w, 31u)] * aWeights.w;
    }
    gl_Position = uLightSpaceMatrix * uModel * skinMat * vec4(aPos, 1.0);
}
