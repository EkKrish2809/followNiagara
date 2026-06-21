#version 450 core

// #extension GL_EXT_shader_16bit_storage: require
// #extension GL_EXT_shader_8bit_storage: require
// #extension GL_EXT_shader_explicit_arithmetic_types: require

// struct Vertex{
//     float vx, vy, vz;
//     uint8_t nx, ny, nz, nw;
//     float tu, tv;
// };

// layout(binding=0) readonly buffer Vertices{
//     Vertex vertices[];
// };

layout (location=0) in vec3 position;
layout (location=1) in uvec4 normal;
layout (location=2) in vec2 texccrds;

layout(location=0) out vec4 color;

void main(){
    vec3 normalf = vec3(normal.xyz) / 127.0 - 1.0;

    gl_Position = vec4(position + vec3(0, 0, 0.5), 1.0);

    color = vec4(normalf * 0.5 + vec3(0.5), 1.0);
}