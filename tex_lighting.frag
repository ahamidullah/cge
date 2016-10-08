#version 330

in vec3 t_normal;
in vec3 t_frag_pos;
in vec2 t_tex_coords;
uniform vec3 u_view_pos;
out vec4 t_color;

struct Material {
	sampler2D diffuse;
	sampler2D specular;
	float shininess;
};
uniform Material mat;

struct Dir_Light {
	vec3 direction;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};
uniform Dir_Light dir;

struct Spot_Light {
	vec3 position;
	vec3 direction;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;

	float outer_cutoff;
	float inner_cutoff;
	float constant;
	float linear;
	float quadratic;
};
uniform Spot_Light spot;

#define MAX_NUM_PT_LIGHTS 4
struct Point_Light {    
	bool is_valid;
	vec3 position;

	float constant;
	float linear;
	float quadratic;

	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};
uniform Point_Light points[MAX_NUM_PT_LIGHTS];

vec3 calc_ambient(vec3 light_ambient)
{
	return light_ambient * vec3(texture(mat.diffuse, t_tex_coords));
}

vec3 calc_diffuse(vec3 light_diffuse, vec3 recv_dir)
{
	float diffuse_impact = max(dot(t_normal, recv_dir), 0.0f);
	return light_diffuse * diffuse_impact * vec3(texture(mat.diffuse, t_tex_coords));
}

vec3 calc_specular(vec3 light_specular, vec3 recv_dir, vec3 view_dir)
{
	vec3 reflect_dir = reflect(-recv_dir, t_normal);
	float specular_impact = pow(max(dot(view_dir, reflect_dir), 0.0), mat.shininess);
	//return light_specular * specular_impact * vec3(texture(mat.specular, t_tex_coords));
	return vec3(0);
}

float calc_attenuation(vec3 light_pos, float constant, float linear, float quadratic)
{
	float distance = length(light_pos - t_frag_pos);
	return 1.0f / (constant + linear * distance + quadratic * (distance * distance));
}

float calc_intensity(vec3 recv_dir, vec3 light_dir, float inner_cutoff, float outer_cutoff)
{
	float theta = dot(recv_dir, normalize(-light_dir));
	float epsilon   = inner_cutoff - outer_cutoff;
	return clamp((theta - outer_cutoff) / epsilon, 0.0, 1.0);
}

void main()
{
	vec3 view_dir = normalize(u_view_pos - t_frag_pos);
	vec3 amb_result = vec3(0), pt_result = vec3(0), spot_result = vec3(0);

	// directional lighting
	{
		vec3 amb_recv_dir = normalize(-dir.direction);
		amb_result = calc_ambient(dir.ambient) + calc_diffuse(dir.diffuse, amb_recv_dir) + calc_specular(dir.specular, amb_recv_dir, view_dir);
	}
	// point lights
	{
		vec3 pt_recv_dir;
		for (int i = 0; i < MAX_NUM_PT_LIGHTS; i++) {
			if (points[i].is_valid) {
				pt_recv_dir = normalize(points[i].position - t_frag_pos);
				pt_result += (calc_ambient(points[i].ambient) +
						calc_diffuse(points[i].diffuse, pt_recv_dir) +
						calc_specular(points[i].specular, pt_recv_dir, view_dir)) *
						calc_attenuation(points[i].position, points[i].constant, points[i].linear, points[i].quadratic);
			}
		}
	}
	// spot light
	{
		vec3 spot_recv_dir = normalize(spot.position - t_frag_pos);
		float intensity = calc_intensity(spot_recv_dir, spot.direction, spot.inner_cutoff, spot.outer_cutoff);
		/*spot_result = (calc_ambient(spot.ambient) +
				(calc_diffuse(spot.diffuse, spot_recv_dir) * intensity) +
				(calc_specular(spot.specular, spot_recv_dir, view_dir) * intensity)) *
				calc_attenuation(spot.position, spot.constant, spot.linear, spot.quadratic);*/
	}

	t_color = vec4(amb_result + pt_result + spot_result, 1.0);
}
