#version 300 es
precision highp float;

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;

out vec2 vNdc;

void main() {
    vNdc = aPos;
    gl_Position = vec4(aPos, 0.999, 1.0);
}
