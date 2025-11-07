#version 450 core

// 顶点属性
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aInstanceRow0;
layout(location = 5) in vec4 aInstanceRow1;
layout(location = 6) in vec4 aInstanceRow2;
layout(location = 7) in vec4 aInstanceRow3;

// 输出到片段着色器
out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec4 VertexColor;

// Uniforms
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform bool uHasInstanceData;

void main() {
    mat4 instanceModel = mat4(1.0);
    if (uHasInstanceData) {
        instanceModel = mat4(aInstanceRow0, aInstanceRow1, aInstanceRow2, aInstanceRow3);
    }

    mat4 modelMatrix = uModel * instanceModel;

    // 计算世界空间位置
    vec4 worldPos = modelMatrix * vec4(aPosition, 1.0);
    FragPos = worldPos.xyz;

    // 变换法线到世界空间
    Normal = mat3(transpose(inverse(modelMatrix))) * aNormal;

    // 传递纹理坐标和顶点颜色
    TexCoord = aTexCoord;
    VertexColor = aColor;

    // 计算最终位置
    gl_Position = uProjection * uView * worldPos;
}

