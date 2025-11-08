#version 450 core

in vec2 vTexCoord;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uTextColor;

void main() {
    vec4 glyph = texture(uTexture, vTexCoord);
    float alpha = glyph.a * uTextColor.a;
    vec3 color = glyph.rgb * uTextColor.rgb;
    FragColor = vec4(color, alpha);
}


