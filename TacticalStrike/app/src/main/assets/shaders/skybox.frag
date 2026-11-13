#version 300 es
precision highp float;

in vec2 vNdc;

uniform vec3 uSkyTop;
uniform vec3 uSkyHorizon;
uniform vec3 uCamForward;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform float uTanHalfFov;
uniform float uAspect;

out vec4 fragColor;

void main() {
    vec3 rayDir = normalize(
        uCamForward +
        uCamRight * (vNdc.x * uAspect * uTanHalfFov) +
        uCamUp * (vNdc.y * uTanHalfFov)
    );
    float h = clamp(rayDir.y * 0.5 + 0.5, 0.0, 1.0);
    float upFactor = pow(clamp(rayDir.y, 0.0, 1.0), 0.65);
    vec3 col = mix(uSkyHorizon, uSkyTop, upFactor);
    if (rayDir.y < 0.0) {
        col = mix(uSkyHorizon, uSkyHorizon * 0.45, clamp(-rayDir.y * 2.0, 0.0, 1.0));
    }
    // Subtle tactical horizon glow
    float sunGlow = pow(max(dot(rayDir, normalize(vec3(0.45, 0.35, -0.55))), 0.0), 16.0);
    col += vec3(0.45, 0.32, 0.18) * sunGlow * h;
    fragColor = vec4(pow(col, vec3(2.2)), 1.0);
}
