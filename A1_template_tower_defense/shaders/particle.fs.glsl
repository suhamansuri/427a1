#version 330

// input from vertex

uniform vec4 color;

layout(location = 0) out vec4 out_color;

void main()
{
    float dist = length(gl_PointCoord - vec2(0.5));
    if (dist > 0.5) discard; // circle
    out_color = color; 
}