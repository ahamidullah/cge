#version 330

in vec3 t_normal;
in vec3 t_frag_pos;
uniform vec3 u_view_pos;
out vec4 t_color;

struct Material {
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	float shininess;
};
uniform Material material;

struct Dir_Light {
	vec3 direction;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};
uniform Dir_Light dir_light;

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
uniform Spot_Light spot_light;

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
uniform Point_Light point_lights[MAX_NUM_PT_LIGHTS];

vec3 calc_ambient(vec3 light_ambient)
{
	return light_ambient * material.ambient;
}

vec3 calc_diffuse(vec3 light_diffuse, vec3 recv_dir)
{
	float diffuse_impact = max(dot(t_normal, recv_dir), 0.0f);
	return light_diffuse * diffuse_impact * material.diffuse;
}

vec3 calc_specular(vec3 light_specular, vec3 recv_dir, vec3 view_dir)
{
	vec3 reflect_dir = reflect(-recv_dir, t_normal);
	float specular_impact = pow(max(dot(view_dir, reflect_dir), 0.0), material.shininess);
	return light_specular * specular_impact * material.specular;
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
		vec3 amb_recv_dir = normalize(-dir_light.direction);
		amb_result = calc_ambient(dir_light.ambient) +
		             calc_diffuse(dir_light.diffuse, amb_recv_dir) +
		             calc_specular(dir_light.specular, amb_recv_dir, view_dir);
	}
	// point lights
	{
		vec3 pt_recv_dir;
		for (int i = 0; i < MAX_NUM_PT_LIGHTS; i++) {
			if (point_lights[i].is_valid) {
				pt_recv_dir = normalize(point_lights[i].position - t_frag_pos);
				pt_result += (calc_ambient(point_lights[i].ambient) +
						calc_diffuse(point_lights[i].diffuse, pt_recv_dir) +
						calc_specular(point_lights[i].specular, pt_recv_dir, view_dir)) *
						calc_attenuation(point_lights[i].position, point_lights[i].constant, point_lights[i].linear, point_lights[i].quadratic);
			}
		}
	}

	// spot light
	{
		vec3 spot_recv_dir = normalize(spot_light.position - t_frag_pos);
		float intensity = calc_intensity(spot_recv_dir, spot_light.direction, spot_light.inner_cutoff, spot_light.outer_cutoff);
		spot_result = (calc_ambient(spot_light.ambient) +
				(calc_diffuse(spot_light.diffuse, spot_recv_dir) * intensity) +
				(calc_specular(spot_light.specular, spot_recv_dir, view_dir) * intensity)) *
				calc_attenuation(spot_light.position, spot_light.constant, spot_light.linear, spot_light.quadratic);
	}

	t_color = vec4(amb_result + pt_result + spot_result, 1.0);
}
