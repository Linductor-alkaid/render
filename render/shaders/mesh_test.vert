#version 450 core

// 顶点属性
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;

// 输出到片段着色器
out vec3 Normal;
out vec4 VertexColor;

// Uniforms
uniform mat4 uMVP;

void main() {
    // 传递法线和顶点颜色
    Normal = aNormal;
    VertexColor = aColor;
    
    // 计算最终位置
    gl_Position = uMVP * vec4(aPosition, 1.0);
}

