#version 330

// input
in vec3 in_position;

// uniform data
uniform mat3 transform;
uniform mat3 projection;
uniform float point_size;

out vec4 frag_color;

void main()
{
    vec3 pos = projection * transform * vec3(in_position.xy, 1.0);
    gl_Position = vec4(pos.xy, in_position.z, 1.0);
    gl_PointSize = point_size;

}