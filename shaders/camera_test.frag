#version 450 core

in vec3 vFragPos;
in vec3 vNormal;
in vec4 vColor;

out vec4 FragColor;

uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform vec3 uLightColor;

void main() {
    // 环境光
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * uLightColor;
    
    // 漫反射光
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vFragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor;
    
    // 镜面光
    float specularStrength = 0.5;
    vec3 viewDir = normalize(uViewPos - vFragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * uLightColor;
    
    // 合成
    vec3 result = (ambient + diffuse + specular) * vColor.rgb;
    FragColor = vec4(result, vColor.a);
}

