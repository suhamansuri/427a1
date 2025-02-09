#version 330

uniform vec3 color;

// Output color
layout(location = 0) out vec4 out_color;

void main()
{
	float dist = length(gl_PointCoord - vec2(0.5));
    if (dist > 0.5) 
        discard;
	out_color = vec4(color, 1.0);
}
