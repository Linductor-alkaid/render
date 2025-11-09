#version 450 core

in vec3 FragPos;
in vec2 TexCoord;
in vec3 Normal;
in vec3 Tangent;
in vec3 Bitangent;
in vec4 VertexColor;

out vec4 FragColor;

uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
uniform bool hasDiffuseMap;
uniform bool hasNormalMap;
uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform vec3 uAmbientColor;
uniform vec3 uDiffuseColor;
uniform vec3 uSpecularColor;
uniform float uShininess;

void main() {
    vec3 baseColor = uDiffuseColor;
    if (hasDiffuseMap) {
        baseColor *= texture(diffuseMap, TexCoord).rgb;
    }
    baseColor *= VertexColor.rgb;

    vec3 N = normalize(Normal);
    if (hasNormalMap) {
        vec3 sampled = texture(normalMap, TexCoord).rgb;
        vec3 tangentNormal = sampled * 2.0 - 1.0;
        mat3 TBN = mat3(normalize(Tangent), normalize(Bitangent), N);
        N = normalize(TBN * tangentNormal);
    }

    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uViewPos - FragPos);

    vec3 ambient = uAmbientColor * baseColor;
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * baseColor;

    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uShininess);
    vec3 specular = spec * uSpecularColor;

    vec3 color = ambient + diffuse + specular;
    FragColor = vec4(color, 1.0);
}

