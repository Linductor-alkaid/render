#version 450 core

// 顶点属性
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;

// 输出到片段着色器
out vec3 FragPos;
out vec3 Normal;
out vec4 VertexColor;

// Uniforms
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    // 世界空间位置
    FragPos = vec3(uModel * vec4(aPosition, 1.0));
    
    // 世界空间法线（需要法线矩阵，这里简化处理）
    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    
    // 传递顶点颜色
    VertexColor = aColor;
    
    // 计算最终位置
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}

