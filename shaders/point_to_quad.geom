#version 450 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in VS_OUT {
    vec4 color;
} gs_in[];

out vec4 fragColor;

uniform float quadSize = 0.1;

void main()
{
    vec4 position = gl_in[0].gl_Position;
    vec4 color = gs_in[0].color;
    
    // 生成四个顶点构成一个四边形（以点为中心）
    
    // 左下
    gl_Position = position + vec4(-quadSize, -quadSize, 0.0, 0.0);
    fragColor = color;
    EmitVertex();
    
    // 右下
    gl_Position = position + vec4(quadSize, -quadSize, 0.0, 0.0);
    fragColor = color;
    EmitVertex();
    
    // 左上
    gl_Position = position + vec4(-quadSize, quadSize, 0.0, 0.0);
    fragColor = color;
    EmitVertex();
    
    // 右上
    gl_Position = position + vec4(quadSize, quadSize, 0.0, 0.0);
    fragColor = color;
    EmitVertex();
    
    EndPrimitive();
}

