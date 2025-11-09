#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec3 aTangent;
layout(location = 5) in vec3 aBitangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 FragPos;
out vec2 TexCoord;
out vec3 Normal;
out vec3 Tangent;
out vec3 Bitangent;
out vec4 VertexColor;

void main() {
    mat3 normalMatrix = mat3(transpose(inverse(uModel)));

    vec3 T = normalize(normalMatrix * aTangent);
    vec3 B = normalize(normalMatrix * aBitangent);
    vec3 N = normalize(normalMatrix * aNormal);

    FragPos = vec3(uModel * vec4(aPosition, 1.0));
    TexCoord = aTexCoord;
    Normal = N;
    Tangent = T;
    Bitangent = B;
    VertexColor = aColor;

    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}

