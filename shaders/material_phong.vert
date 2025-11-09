#version 450 core

// 顶点属性
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec3 aTangent;
layout(location = 5) in vec3 aBitangent;

// 输出到片段着色器
out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 VertexColor;
out vec3 Tangent;
out vec3 Bitangent;

// Uniforms
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

const int MAX_EXTRA_UV_SETS = 4;
uniform int uExtraUVSetCount;
uniform vec2 uExtraUVSetScales[MAX_EXTRA_UV_SETS];

void main() {
    // 世界空间位置
    FragPos = vec3(uModel * vec4(aPosition, 1.0));
    
    mat3 normalMatrix = mat3(transpose(inverse(uModel)));
    
    // 世界空间法线与切线空间
    Normal = normalize(normalMatrix * aNormal);
    Tangent = normalize(normalMatrix * aTangent);
    Bitangent = normalize(normalMatrix * aBitangent);
    
    // 传递纹理坐标（支持多个额外缩放集合）
    vec2 adjustedUV = aTexCoord;
    for (int i = 0; i < uExtraUVSetCount && i < MAX_EXTRA_UV_SETS; ++i) {
        adjustedUV *= uExtraUVSetScales[i];
    }
    TexCoord = adjustedUV;
    
    // 传递顶点颜色
    VertexColor = aColor;
    
    // 计算最终位置
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}

