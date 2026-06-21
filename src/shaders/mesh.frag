#version 450 core

layout(location=0) out vec4 outputColor;

layout(location=0) in vec4 color;

void main(){
    outputColor = color;
    // outputColor = vec4(1.0, 0, 0, 1);
}