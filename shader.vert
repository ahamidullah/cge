#version 330

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 in_color;
uniform float loop_duration;
uniform float time;

smooth out vec4 vert_color;

void main()
{
	float time_scale = 3.14159f * 2.0f / loop_duration;
	float cur_time = mod(time, loop_duration);
	vec4 total_offset = vec4(cos(cur_time * time_scale) * 0.5f, sin(cur_time * time_scale) * 0.5f, 0.0f, 0.0f);
	gl_Position = pos + total_offset;
	vert_color = in_color;
}
