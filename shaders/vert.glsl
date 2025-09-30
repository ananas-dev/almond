#version 450

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;

layout (location = 0) out vec2 vUV;

layout(std140, set = 1, binding = 0) uniform VertexUniforms {
    mat4 proj_view;
    mat4 model;
};

void main() {
    vUV = aUV;
    gl_Position = proj_view * model * vec4(aPos, 1.0);
}
