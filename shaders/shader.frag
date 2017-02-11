out vec4 o_color;

#ifdef UI
	in vec3 t_color;
	void main()
	{
		o_color = vec4(t_color, 1.0f);
	}

#elif defined(TEX) || defined(NO_TEX)
	in vec3 t_normal;
	in vec3 t_frag_pos;
	uniform vec3 u_view_pos;
#  ifdef TEX
	in vec2 t_tex_coords;
	struct Material {
		sampler2D diffuse;
		sampler2D specular;
		float shininess;
	};
#  else
	struct Material {
		vec3 ambient;
		vec3 diffuse;
		vec3 specular;
		float shininess;
	};
#  endif
	uniform Material mat;

	struct Dir_Light {
		vec4 direction;
		vec4 ambient;
		vec4 diffuse;
		vec4 specular;
	};
	
	struct Spot_Light {
		// TODO: pack the floats into the last component of the vec4
		vec4 position;
		vec4 direction;
		vec4 ambient;
		vec4 diffuse;
		vec4 specular;

		float outer_cutoff;
		float inner_cutoff;
		float constant;
		float linear;
		float quadratic;
	};

	#define MAX_NUM_PT_LIGHTS 4
	struct Point_Light {
		vec4 position;
		vec4 ambient;
		vec4 diffuse;
		vec4 specular;
		float constant;
		float linear;
		float quadratic;
		bool is_valid;
	};

	layout (std140) uniform Lights
	{
		Dir_Light dir;
		Spot_Light spot;
		Point_Light points[MAX_NUM_PT_LIGHTS];
	};

	vec3 ambient(vec3 intensity)
	{
#  ifdef NO_TEX
		return intensity * mat.ambient;
#  else
		return intensity * vec3(texture(mat.diffuse, t_tex_coords));
#  endif
	}

	vec3 diffuse(vec3 intensity, vec3 recv_dir)
	{
		float impact = max(dot(t_normal, recv_dir), 0.0f);
#  ifdef NO_TEX
		return intensity * impact * mat.diffuse;
#  else
		return intensity * impact * vec3(texture(mat.diffuse, t_tex_coords));
#  endif
	}
	
	vec3 specular(vec3 intensity, vec3 recv_dir, vec3 view_dir)
	{
		vec3 reflect_dir = reflect(-recv_dir, t_normal);
		float impact = pow(max(dot(view_dir, reflect_dir), 0.0), mat.shininess);
#  ifdef NO_TEX
		return intensity * impact * mat.specular;
#  else
		return intensity * impact * vec3(texture(mat.specular, t_tex_coords));
#  endif
	}
	
	float attenuation(vec3 light_pos, float constant, float linear, float quadratic)
	{
		float distance = length(light_pos - t_frag_pos);
		return 1.0f / (constant + linear * distance + quadratic * (distance * distance));
	}
	
	float intensity(vec3 recv_dir, vec3 light_dir, float inner_cutoff, float outer_cutoff)
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
			vec3 recv_dir = normalize(-dir.direction.xyz);
			dir_result = ambient(dir.ambient.xyz) +
			             diffuse(dir.diffuse.xyz, recv_dir) +
			             specular(dir.specular.xyz, recv_dir, view_dir);
		}
		// point lights
		{
			vec3 recv_dir;
			for (int i = 0; i < MAX_NUM_PT_LIGHTS; i++) {
				if (points[i].is_valid) {
					recv_dir = normalize(points[i].position.xyz - t_frag_pos);
					pt_result += (ambient(points[i].ambient.xyz) +
							diffuse(points[i].diffuse.xyz, recv_dir) +
							specular(points[i].specular.xyz, recv_dir, view_dir)) *
							attenuation(points[i].position.xyz, points[i].constant, points[i].linear, points[i].quadratic);
				}
			}
		}
	
		// spot light
		{
			vec3 recv_dir = normalize(spot.position.xyz - t_frag_pos);
			float intensity = intensity(recv_dir, spot.direction.xyz, spot.inner_cutoff, spot.outer_cutoff);
			spot_result = (ambient(spot.ambient.xyz) +
					(diffuse(spot.diffuse.xyz, recv_dir) * intensity) +
					(specular(spot.specular.xyz, recv_dir, view_dir) * intensity)) *
					attenuation(spot.position.xyz, spot.constant, spot.linear, spot.quadratic);
		}
	
		o_color = vec4(dir_result + pt_result + spot_result, 1.0);
	}

#else
#error "Missing shader lighting type: UI, NO_TEX, or TEX."
#endif
