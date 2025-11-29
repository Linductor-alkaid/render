#version 450 core

// 顶点属性
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec3 aTangent;
layout(location = 5) in vec3 aBitangent;

// 实例化属性
layout(location = 6) in vec4 aInstanceRow0;
layout(location = 7) in vec4 aInstanceRow1;
layout(location = 8) in vec4 aInstanceRow2;
layout(location = 9) in vec4 aInstanceRow3;
layout(location = 10) in vec4 aInstanceColor;
layout(location = 11) in vec4 aInstanceParams;

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
uniform bool uHasInstanceData;

const int MAX_EXTRA_UV_SETS = 4;
uniform int uExtraUVSetCount;
uniform vec2 uExtraUVSetScales[MAX_EXTRA_UV_SETS];

void main() {
    // 构建实例变换矩阵
    mat4 instanceModel = mat4(1.0);
    if (uHasInstanceData) {
        instanceModel = mat4(aInstanceRow0, aInstanceRow1, aInstanceRow2, aInstanceRow3);
    }
    
    // 组合模型矩阵和实例矩阵
    mat4 modelMatrix = uModel * instanceModel;
    
    // 世界空间位置
    FragPos = vec3(modelMatrix * vec4(aPosition, 1.0));
    
    mat3 normalMatrix = mat3(transpose(inverse(modelMatrix)));
    
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
    
    // 传递顶点颜色（如果使用实例颜色，则混合）
    if (uHasInstanceData) {
        VertexColor = aColor * aInstanceColor;
    } else {
        VertexColor = aColor;
    }
    
    // 计算最终位置
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}

