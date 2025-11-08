#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;
layout(location = 4) in mat4 aInstanceModel;
layout(location = 8) in vec4 aInstanceUVRect;
layout(location = 9) in vec4 aInstanceTint;

out vec2 vTexCoord;
out vec4 vVertexColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec4 uUVRect;      // xy = offset, zw = scale
uniform vec4 uTintColor;
uniform bool uUseInstancing;

void main() {
    mat4 model = uUseInstancing ? aInstanceModel : uModel;
    vec4 uvRect = uUseInstancing ? aInstanceUVRect : uUVRect;
    vec4 tint = uUseInstancing ? aInstanceTint : uTintColor;

    vTexCoord = vec2(uvRect.x + aTexCoord.x * uvRect.z,
                     uvRect.y + aTexCoord.y * uvRect.w);
    vVertexColor = aColor * tint;

    vec4 worldPos = model * vec4(aPosition, 1.0);
    gl_Position = uProjection * uView * worldPos;
}


