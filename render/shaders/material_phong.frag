#version 450 core

// 输入
in vec3 FragPos;
in vec3 Normal;
in vec4 VertexColor;

// 输出
out vec4 FragColor;

// 材质属性
uniform vec4 uAmbientColor;
uniform vec4 uDiffuseColor;
uniform vec4 uSpecularColor;
uniform float uShininess;

// 光照
uniform vec3 uLightPos;
uniform vec3 uViewPos;

void main() {
    // 归一化法线
    vec3 norm = normalize(Normal);
    
    // 1. 环境光
    vec3 ambient = uAmbientColor.rgb;
    
    // 2. 漫反射
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uDiffuseColor.rgb;
    
    // 3. 镜面反射（Phong 模型）
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess);
    vec3 specular = spec * uSpecularColor.rgb;
    
    // 组合所有光照分量
    vec3 result = (ambient + diffuse + specular) * VertexColor.rgb;
    
    // 输出最终颜色
    FragColor = vec4(result, uDiffuseColor.a);
}

