#version 430 core
layout(location = 0) in vec3 aPos;
uniform mat4 uProjView;
uniform mat4 uModel;
out vec3 posWS;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    posWS = wp.xyz;
    gl_Position = uProjView * wp;
}