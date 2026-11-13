#version 300 es
precision highp float;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec4 aTangent;
layout(location = 4) in uvec4 aJoints;
layout(location = 5) in vec4 aWeights;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat4 uLightSpaceMatrix;
uniform mat4 uBoneMatrices[32];

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUv;
out vec3 vTangent;
out vec3 vBitangent;
out vec4 vLightSpacePos;

void main() {
    float wSum = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
    mat4 skinMat = mat4(1.0);
    if (wSum > 0.001) {
        skinMat = uBoneMatrices[min(aJoints.x, 31u)] * aWeights.x +
                  uBoneMatrices[min(aJoints.y, 31u)] * aWeights.y +
                  uBoneMatrices[min(aJoints.z, 31u)] * aWeights.z +
                  uBoneMatrices[min(aJoints.w, 31u)] * aWeights.w;
    }

    vec4 localPos = skinMat * vec4(aPos, 1.0);
    vec4 worldPos = uModel * localPos;
    mat3 normalMat = mat3(uModel) * mat3(skinMat);

    vec3 N = normalize(normalMat * aNormal);
    vec3 T = normalize(normalMat * aTangent.xyz);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * aTangent.w;

    vWorldPos = worldPos.xyz;
    vNormal = N;
    vUv = aUv;
    vTangent = T;
    vBitangent = B;
    vLightSpacePos = uLightSpaceMatrix * worldPos;

    gl_Position = uViewProj * worldPos;
}
