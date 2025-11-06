#version 450 core

// 输入
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 VertexColor;

// 输出
out vec4 FragColor;

// Uniforms
uniform vec4 uColor;
uniform sampler2D uTexture0;
uniform bool uUseTexture;
uniform bool uUseVertexColor;

void main() {
    vec4 baseColor = uColor;
    
    // 使用纹理
    if (uUseTexture) {
        baseColor *= texture(uTexture0, TexCoord);
    }
    
    // 使用顶点颜色
    if (uUseVertexColor) {
        baseColor *= VertexColor;
    }
    
    FragColor = baseColor;
}

