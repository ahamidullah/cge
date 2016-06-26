#version 330

//smooth in vec4 vert_color;

out vec4 out_color;

uniform float frag_loop_duration;
uniform float time;

const vec4 first_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
const vec4 second_color = vec4(0.0f, 1.0f, 0.0f, 1.0f);

void main()
{
	float cur_time = mod(time, frag_loop_duration);
	float cur_lerp = cur_time / frag_loop_duration;

	out_color = mix(first_color, second_color, cur_lerp);
}
