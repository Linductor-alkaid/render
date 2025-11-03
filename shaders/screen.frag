#version 450 core

// 屏幕空间片段着色器（用于后处理效果）

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform int postProcessMode;  // 0=None, 1=Grayscale, 2=Invert, 3=Blur, 4=Sharpen

// 卷积核偏移
const float offset = 1.0 / 300.0;
const vec2 offsets[9] = vec2[](
    vec2(-offset,  offset), vec2(0.0,  offset), vec2(offset,  offset),
    vec2(-offset,  0.0),    vec2(0.0,  0.0),    vec2(offset,  0.0),
    vec2(-offset, -offset), vec2(0.0, -offset), vec2(offset, -offset)
);

// 模糊卷积核
const float blurKernel[9] = float[](
    1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0,
    2.0 / 16.0, 4.0 / 16.0, 2.0 / 16.0,
    1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0
);

// 锐化卷积核
const float sharpenKernel[9] = float[](
    -1.0, -1.0, -1.0,
    -1.0,  9.0, -1.0,
    -1.0, -1.0, -1.0
);

void main()
{
    vec4 color = texture(uTexture, TexCoord);
    
    if (postProcessMode == 0) {
        // 无后处理
        FragColor = color;
    }
    else if (postProcessMode == 1) {
        // 灰度
        float average = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
        FragColor = vec4(average, average, average, 1.0);
    }
    else if (postProcessMode == 2) {
        // 反色
        FragColor = vec4(1.0 - color.rgb, 1.0);
    }
    else if (postProcessMode == 3) {
        // 模糊
        vec3 result = vec3(0.0);
        for(int i = 0; i < 9; i++) {
            vec3 sampleColor = texture(uTexture, TexCoord + offsets[i]).rgb;
            result += sampleColor * blurKernel[i];
        }
        FragColor = vec4(result, 1.0);
    }
    else if (postProcessMode == 4) {
        // 锐化
        vec3 result = vec3(0.0);
        for(int i = 0; i < 9; i++) {
            vec3 sampleColor = texture(uTexture, TexCoord + offsets[i]).rgb;
            result += sampleColor * sharpenKernel[i];
        }
        FragColor = vec4(result, 1.0);
    }
    else {
        // 默认
        FragColor = color;
    }
}

