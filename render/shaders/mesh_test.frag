#version 450 core

// 输入
in vec3 Normal;
in vec4 VertexColor;

// 输出
out vec4 FragColor;

// Uniforms
uniform vec4 uColor;
uniform vec3 uLightDir;

void main() {
    // 简单的漫反射光照
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(-uLightDir);
    float diff = max(dot(norm, lightDir), 0.0);
    
    // 环境光 + 漫反射
    float ambient = 0.3;
    float lighting = ambient + (1.0 - ambient) * diff;
    
    // 应用光照到颜色
    vec4 finalColor = uColor * VertexColor;
    FragColor = vec4(finalColor.rgb * lighting, finalColor.a);
}

