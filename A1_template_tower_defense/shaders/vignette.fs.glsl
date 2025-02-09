#version 330

uniform sampler2D screen_texture;
uniform float time;
uniform float darken_screen_factor;
uniform float vignette_intensity;

in vec2 texcoord;

layout(location = 0) out vec4 color;

vec4 vignette(vec4 in_color) 
{
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A1: HANDLE THE VIGNETTE EFFECT HERE
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	if (vignette_intensity <= 0.0) {
        return in_color;  
    }
	vec2 screen_center = vec2(0.5, 0.5);

	float distance = length(texcoord - screen_center) * 2.0;

	float vingette_strength = smoothstep(0.3, 0.75, distance);

	vec4 vingette_color = vec4(1.0, 0.0, 0.0, 1.0); // red

	return mix(in_color, vingette_color, vingette_strength * 0.5 * vignette_intensity);
}

// darken the screen, i.e., fade to black
vec4 fade_color(vec4 in_color) 
{
	if (darken_screen_factor > 0)
		in_color -= darken_screen_factor * vec4(0.8, 0.8, 0.8, 0);
	return in_color;
}

void main()
{
    vec4 in_color = texture(screen_texture, texcoord);
	vec4 vignette_color = vignette(in_color);
	color = fade_color(vignette_color);
    //color = vignette(fade_color(in_color));
}