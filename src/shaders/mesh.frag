#version 450 core


layout(location=0) out vec4 outputColor;

layout(location=0) in vec4 color;

void main(){
    outputColor = color;
    // outputColor = vec4(1.0, 0, 0, 1);
}

/*
#extension GL_NV_mesh_shader : require

layout(location=1) perprimitiveNV in vec3 triangleNormal;

outputColor = vec4(triangleNormal * 0.5 + 0.5, 1.0);

*/