#version 450 core

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types: require

#extension GL_GOOGLE_include_directive : require

#include "mesh.h"

layout(push_constant) uniform block{
    MeshDraw meshDraw;
};

layout(binding=0) readonly buffer Vertices{
    Vertex vertices[];
};

layout(location=0) out vec4 color;

void main(){

    // Vertex v = vertices[gl_VertexIndex];

    vec3 position = vec3(vertices[gl_VertexIndex].vx, vertices[gl_VertexIndex].vy, vertices[gl_VertexIndex].vz);
    vec3 normal = vec3(int(vertices[gl_VertexIndex].nx), int(vertices[gl_VertexIndex].ny), int(vertices[gl_VertexIndex].nz)) / 127.0 - 1.0;
    vec2 texccords = vec2(vertices[gl_VertexIndex].tu, vertices[gl_VertexIndex].tv);

    gl_Position = meshDraw.projection * vec4(rotateQuat(position, meshDraw.orientation) * vec3(meshDraw.scale) + meshDraw.position.xyz, 1);

    color = vec4(normal * 0.5 + vec3(0.5), 1.0);
}