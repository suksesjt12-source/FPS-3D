#version 300 es
precision highp float;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec4 aTangent;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat4 uLightSpaceMatrix;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUv;
out vec3 vTangent;
out vec3 vBitangent;
out vec4 vLightSpacePos;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    mat3 normalMat = mat3(uModel);

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
