#version 300 es
precision mediump float;

in vec2 vUv;
in vec4 vColor;

uniform sampler2D uParticleTex;

out vec4 fragColor;

void main() {
    vec4 tex = texture(uParticleTex, vUv);
    vec4 col = tex * vColor;
    if (col.a < 0.01) discard;
    fragColor = col;
}
