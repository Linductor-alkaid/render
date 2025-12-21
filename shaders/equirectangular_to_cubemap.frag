#version 450 core

// 等距柱状投影到立方体贴图转换 - 片段着色器
// 从等距柱状投影纹理采样，转换为立方体贴图方向

in vec3 WorldPos;
out vec4 FragColor;

uniform sampler2D uEquirectangularMap;

const float PI = 3.14159265359;

// 将3D方向向量转换为等距柱状投影的UV坐标
vec2 SampleSphericalMap(vec3 v)
{
    // 计算球面坐标
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    // 归一化到 [0, 1]
    uv *= vec2(0.1591, 0.3183); // 1/(2*PI) 和 1/PI
    uv += 0.5;
    return uv;
}

void main()
{
    // 归一化世界空间位置（立方体顶点位置）
    vec3 N = normalize(WorldPos);
    
    // 从等距柱状投影纹理采样
    vec2 uv = SampleSphericalMap(N);
    vec3 color = texture(uEquirectangularMap, uv).rgb;
    
    FragColor = vec4(color, 1.0);
}

