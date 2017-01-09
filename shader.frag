out vec4 t_color;

#ifdef NO_LIGHT
	void main()
	{
		t_color = vec4(1.0f); // Set all 4 vector values to 1.0f
	}

#elif defined(TEX) || defined(NO_TEX)
	in vec3 t_normal;
	in vec3 t_frag_pos;
	uniform vec3 u_view_pos;
#ifdef TEX
	in vec2 t_tex_coords;
	struct Material {
		sampler2D diffuse;
		sampler2D specular;
		float shininess;
	};
#else
	struct Material {
		vec3 ambient;
		vec3 diffuse;
		vec3 specular;
		float shininess;
	};
#endif
	uniform Material mat;

	struct Dir_Light {
		vec3 direction;
		vec3 ambient;
		vec3 diffuse;
		vec3 specular;
	};
	
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

	layout (std140) uniform Lights
	{
		Dir_Light dir;
		Spot_Light spot;
		Point_Light points[MAX_NUM_PT_LIGHTS];
	};

	vec3 calc_ambient(vec3 intensity)
	{
#ifdef NO_TEX
		return intensity * mat.ambient;
#else
		return intensity * vec3(texture(mat.diffuse, t_tex_coords));
#endif
	}
	
	vec3 calc_diffuse(vec3 intensity, vec3 recv_dir)
	{
		float impact = max(dot(t_normal, recv_dir), 0.0f);
#ifdef NO_TEX
		return intensity * impact * mat.diffuse;
#else
		return intensity * impact * vec3(texture(mat.diffuse, t_tex_coords));
#endif
	}
	
	vec3 calc_specular(vec3 intensity, vec3 recv_dir, vec3 view_dir)
	{
		vec3 reflect_dir = reflect(-recv_dir, t_normal);
		float impact = pow(max(dot(view_dir, reflect_dir), 0.0), mat.shininess);
#ifdef NO_TEX
		return intensity * impact * mat.specular;
#else
		//return light_specular * specular_impact * vec3(texture(mat.specular, t_tex_coords));
		return vec3(0);
#endif
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
		vec3 dir_result = vec3(0), pt_result = vec3(0), spot_result = vec3(0);
	
		// directional lighting
		{
			vec3 recv_dir = normalize(-dir.direction);
			dir_result = calc_ambient(dir.ambient) +
			             calc_diffuse(dir.diffuse, recv_dir) +
			             calc_specular(dir.specular, recv_dir, view_dir);
		}
		// point lights
		{
			vec3 recv_dir;
			for (int i = 0; i < MAX_NUM_PT_LIGHTS; i++) {
				if (points[i].is_valid) {
					recv_dir = normalize(points[i].position - t_frag_pos);
					pt_result += (calc_ambient(points[i].ambient) +
							calc_diffuse(points[i].diffuse, recv_dir) +
							calc_specular(points[i].specular, recv_dir, view_dir)) *
							calc_attenuation(points[i].position, points[i].constant, points[i].linear, points[i].quadratic);
				}
			}
		}
	
		// spot light
		{
			vec3 recv_dir = normalize(spot.position - t_frag_pos);
			float intensity = calc_intensity(recv_dir, spot.direction, spot.inner_cutoff, spot.outer_cutoff);
			spot_result = (calc_ambient(spot.ambient) +
					(calc_diffuse(spot.diffuse, recv_dir) * intensity) +
					(calc_specular(spot.specular, recv_dir, view_dir) * intensity)) *
					calc_attenuation(spot.position, spot.constant, spot.linear, spot.quadratic);
		}
	
		t_color = vec4(dir_result + pt_result + spot_result, 1.0);
	}

#else
#error "Missing shader lighting type: NO_LIGHT, NO_TEX, or TEX."
#endif
