#version 450 core

// 输入
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 VertexColor;

// 输出
out vec4 FragColor;

// Uniforms
uniform vec4 color;
uniform sampler2D texture0;
uniform bool useTexture;
uniform bool useVertexColor;

void main() {
    vec4 baseColor = color;
    
    // 使用纹理
    if (useTexture) {
        baseColor *= texture(texture0, TexCoord);
    }
    
    // 使用顶点颜色
    if (useVertexColor) {
        baseColor *= VertexColor;
    }
    
    FragColor = baseColor;
}

