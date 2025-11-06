#version 450 core

// 顶点属性
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;

// 输出到片段着色器
out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec4 VertexColor;

// Uniforms
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    // 计算世界空间位置
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    FragPos = worldPos.xyz;
    
    // 变换法线到世界空间
    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    
    // 传递纹理坐标和顶点颜色
    TexCoord = aTexCoord;
    VertexColor = aColor;
    
    // 计算最终位置
    gl_Position = uProjection * uView * worldPos;
}

