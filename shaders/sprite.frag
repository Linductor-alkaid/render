#version 450 core

in vec2 vTexCoord;
in vec4 vVertexColor;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform bool uUseTexture;

void main() {
    vec4 color = vVertexColor;
    if (uUseTexture) {
        color *= texture(uTexture, vTexCoord);
    }

    FragColor = color;
}


