#version 300 es
precision highp float;

in vec2 vUv;

uniform sampler2D uSceneTex;
uniform float uDamageFlash;
uniform float uAdsFactor;

out vec4 fragColor;

vec3 acesToneMap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(uSceneTex, vUv).rgb;
    vec3 mapped = acesToneMap(hdr * 1.15);
    vec3 srgb = pow(mapped, vec3(1.0 / 2.2));

    vec2 centered = vUv * 2.0 - 1.0;
    float dist = length(centered);
    float vigStrength = mix(0.28, 0.52, uAdsFactor);
    float vig = smoothstep(0.45, 1.35, dist) * vigStrength;
    srgb *= (1.0 - vig);

    if (uDamageFlash > 0.01) {
        float edge = smoothstep(0.35, 1.15, dist) * uDamageFlash * 0.65;
        srgb = mix(srgb, vec3(0.85, 0.08, 0.06), edge);
    }

    fragColor = vec4(srgb, 1.0);
}
