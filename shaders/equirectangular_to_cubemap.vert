#version 450 core

// 等距柱状投影到立方体贴图转换 - 顶点着色器
// 渲染一个立方体，每个面对应立方体贴图的一个面

layout (location = 0) in vec3 aPos;

out vec3 WorldPos;

uniform mat4 uProjection;
uniform mat4 uView;

void main()
{
    // 将顶点位置转换为世界空间方向向量
    WorldPos = aPos;
    // 使用视图和投影矩阵
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}

