#version 450 core

// 输入
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 VertexColor;

// 输出
out vec4 FragColor;

// 材质属性
uniform vec4 uAmbientColor;
uniform vec4 uDiffuseColor;
uniform vec4 uSpecularColor;
uniform float uShininess;

// 纹理
uniform sampler2D diffuseMap;
uniform bool hasDiffuseMap;

// 光照
uniform vec3 uLightPos;
uniform vec3 uViewPos;

void main() {
    // 归一化法线
    vec3 norm = normalize(Normal);
    
    // 获取漫反射颜色（纹理或材质颜色）
    vec4 diffuseColor = uDiffuseColor;
    if (hasDiffuseMap) {
        vec4 texColor = texture(diffuseMap, TexCoord);
        diffuseColor = texColor * uDiffuseColor;  // 纹理颜色与材质颜色相乘
    }
    
    // 1. 环境光
    vec3 ambient = uAmbientColor.rgb * diffuseColor.rgb;
    
    // 2. 漫反射
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * diffuseColor.rgb;
    
    // 3. 镜面反射（Phong 模型）
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess);
    vec3 specular = spec * uSpecularColor.rgb;
    
    // 组合所有光照分量
    vec3 result = (ambient + diffuse + specular) * VertexColor.rgb;
    
    // 输出最终颜色
    FragColor = vec4(result, diffuseColor.a);
}

