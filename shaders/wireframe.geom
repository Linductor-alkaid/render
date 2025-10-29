#version 450 core

layout (triangles) in;
layout (line_strip, max_vertices = 4) out;

in VS_OUT {
    vec3 normal;
    vec3 fragPos;
} gs_in[];

out vec3 fragColor;

uniform vec3 wireframeColor = vec3(0.0, 1.0, 0.0);

void main()
{
    // 绘制三角形的三条边
    
    // 第一条边
    gl_Position = gl_in[0].gl_Position;
    fragColor = wireframeColor;
    EmitVertex();
    
    // 第二条边
    gl_Position = gl_in[1].gl_Position;
    fragColor = wireframeColor;
    EmitVertex();
    
    // 第三条边
    gl_Position = gl_in[2].gl_Position;
    fragColor = wireframeColor;
    EmitVertex();
    
    // 闭合三角形
    gl_Position = gl_in[0].gl_Position;
    fragColor = wireframeColor;
    EmitVertex();
    
    EndPrimitive();
}

