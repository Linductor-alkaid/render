#version 450 core

// 屏幕空间顶点着色器（用于后处理）

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;  // 修正：Mesh的texCoord在location 1

out vec2 TexCoord;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    // 帧缓冲纹理需要翻转Y轴（OpenGL帧缓冲坐标系统）
    TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
}

