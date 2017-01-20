#include <stdio.h>
#include <GL/glew.h>
#include <assert.h>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#define GLM_SWIZZLE
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <stdint.h>
#include "render.h"

constexpr int MAX_PT_LIGHTS = 4;
bool points[MAX_PT_LIGHTS];

struct Render_Data {
	glm::vec3 pos;
	GLuint vao;
	GLuint ibo;
	size_t num_indices;
};

struct Dir_Light {
	glm::vec4 direction;
	glm::vec4 ambient;
	glm::vec4 diffuse;
	glm::vec4 specular;
};

struct Spot_Light {
	// TODO: pack the floats into the last component of the glm::vec4
	glm::vec4 position;
	glm::vec4 direction;
	glm::vec4 ambient;
	glm::vec4 diffuse;
	glm::vec4 specular;

	float outer_cutoff;
	float inner_cutoff;
	float constant;
	float linear;
	float quadratic;
};

struct Point_Light {    
	glm::vec4 position;
	glm::vec4 ambient;
	glm::vec4 diffuse;
	glm::vec4 specular;
	float constant;
	float linear;
	float quadratic;
	bool is_valid;
};

// cpu copy of shader ubo light structures
struct Lights {
	alignas(16) Dir_Light dir;
	alignas(16) Spot_Light spot;
	alignas(16) Point_Light points[MAX_PT_LIGHTS];
} g_lights;

static GLuint g_tex, g_spec_tex;

struct Render_Group {
	std::vector<Render_Data> tex;
	std::vector<Render_Data> notex;
	std::vector<Render_Data> nolight;
} g_render_groups;

struct UBO_IDs {
	GLuint matrices;
	GLuint lights;
} g_ubos;

struct Shaders {
	GLuint tex;
	GLuint notex;
	GLuint nolight;
} g_shaders;

struct Matrices {
	glm::mat4 proj;
	glm::mat4 view;
} g_matrices;

enum GL_Buffers {
	vert_buf = 0,
	norm_buf,
	ind_buf,
	tex_buf,
	num_buffers
};

struct Model {
	std::optional<std::string> tex_fname;
	std::vector<GLfloat> vertices;
	std::vector<GLfloat> tex_coords;
	std::vector<GLfloat> normals;
	std::vector<GLuint> indices;
};

std::optional<glm::vec3>
raycast_plane(const glm::vec2 &screen_ray, const glm::vec3 &plane_normal, const glm::vec3 &origin, const float origin_ofs, const Screen &scr)
{
	float x = (2.0f * screen_ray.x) / scr.w - 1.0f;
	float y = 1.0f - (2.0f * screen_ray.y) / scr.h;
	glm::vec4 clip_ray = glm::vec4(x, y, -1.0f, 1.0f);
//	glm::mat4 projection = glm::perspective(fov, screen_width / screen_height, near_plane, far_plane);
	glm::vec4 eye_ray = glm::inverse(g_matrices.proj) * clip_ray;
	eye_ray = glm::vec4(eye_ray.x, eye_ray.y, -1.0f, 0.0f);
//	glm::mat4 view = glm::lookAt(cam.pos, cam.pos + cam.front, cam.up);
	glm::vec3 world_ray = glm::normalize((glm::inverse(g_matrices.view) * eye_ray).xyz());
	float l = glm::dot(world_ray, plane_normal);
	if (l >= 0.0f && l <= 0.001f) // perpendicular
		return {};
	float t = -((glm::dot(origin, glm::vec3(0.0f, 1.0f, 0.0f)) + origin_ofs) / l);
	if (t < 0.0f) // intersected behind
		return {};
	glm::vec3 pos = origin + world_ray * t;
	return pos;
}

void
mk_point_light(glm::vec3 pos)
{
	// TODO: stick lighting data in a uniform buffer obj
//	glUseProgram(g_tex_lighting_prog);
//	for (int i = 0; i < max_pt_lights; ++i) {
//		if (!points[i]) {
//			points[i] = true;
//			char buf[256];
//			sprintf(buf, "points[%d].is_valid", i);
//			glUniform1i(glGetUniformLocation(g_tex_lighting_prog, buf), true);
//			sprintf(buf, "points[%d].position", i);
//			glUniform3f(glGetUniformLocation(g_tex_lighting_prog, buf), pos.x, pos.y + 0.5f, pos.z);
//			sprintf(buf, "points[%d].ambient", i);
//			glUniform3f(glGetUniformLocation(g_tex_lighting_prog, buf), 0.05f, 0.05f, 0.05f);
//			sprintf(buf, "points[%d].diffuse", i);
//			glUniform3f(glGetUniformLocation(g_tex_lighting_prog, buf), 0.8f, 0.8f, 0.8f);
//			sprintf(buf, "points[%d].specular", i);
//			glUniform3f(glGetUniformLocation(g_tex_lighting_prog, buf), 1.0f, 1.0f, 1.0f);
//			sprintf(buf, "points[%d].constant", i);
//			glUniform1f(glGetUniformLocation(g_tex_lighting_prog, buf), 1.0f);
//			sprintf(buf, "points[%d].linear", i);
//			glUniform1f(glGetUniformLocation(g_tex_lighting_prog, buf), 0.09);
//			sprintf(buf, "points[%d].quadratic", i);
//			glUniform1f(glGetUniformLocation(g_tex_lighting_prog, buf), 0.032);
//			glUseProgram(0);
//			return;
//		}
//	}
//	zlog_err("Max point lights reached!");
//	glUseProgram(0);
}

// hacky obj (sort of) loader
bool
load_model(const char *fname, Model *mi)
{
	std::optional<std::string> load = load_file(fname, "r");
	if (!load)
		return false;
	const std::string &data = load.value();

	size_t num_verts = 0;
	size_t num_norms = 0;
	size_t num_inds = 0;
	size_t num_texcs = 0;

	for (uint64_t i = 0; data[i]; ++i) {
		if (data[i] == 'v') {
			if (data[i+1] == 't')
				num_texcs += 2;
			else if (data[i+1] == 'n')
				num_norms += 3;
			else if (data[i+1] == ' ')
				num_verts += 3;
		} else if (data[i] == 'f') {
			num_inds += 3;
		}
		while (data[i] && data[i] != '\n') ++i;
	}

	mi->vertices.reserve(num_verts);
	mi->normals.reserve(num_norms);
	mi->indices.reserve(num_inds);
	mi->tex_coords.reserve(num_texcs);
	mi->tex_fname = {};
		
	for (uint64_t i = 0; data[i]; ++i) {
		if (data[i] == 't' && data[i+1] == 'f') {
			char tex[256];
			// doesn't work if texture filename has spaces...
			if (sscanf(&data[i], "tf %s", tex) < 1) {
				zerror("obj error: could not parse texture file name in file %s\n", fname);
				return false;
			}
			mi->tex_fname = std::string(tex);
		}
		if (data[i] == 'v') {
			if (data[i+1] == 't') {
				GLfloat t1, t2;
				if (sscanf(&data[i+2], "%f %f", &t1, &t2) < 2) {
					zerror("obj parser error in file %s\n", fname);
					return false;
				}
				mi->tex_coords.insert(mi->tex_coords.end(), { t1, t2 });
			} else {
				GLfloat v1, v2, v3;
				if (sscanf(&data[i+2], "%f %f %f", &v1, &v2, &v3) < 3) {
					zerror("obj parser error in file %s\n", fname);
					return false;
				}
				if (data[i+1] == ' ')
					mi->vertices.insert(mi->vertices.end(), { v1, v2, v3 });
				else if (data[i+1] == 'n')
					mi->normals.insert(mi->normals.end(), { v1, v2, v3 });
			}
		} else if (data[i] == 'f') {
			GLuint i1, i2, i3;
			if (sscanf(&data[i+1], "%u %u %u", &i1, &i2, &i3) < 3) {
				zerror("obj parser error in file %s\n", fname);
				return false;
			}
			mi->indices.insert(mi->indices.end(), { i1, i2, i3 });
		}
		while (data[i] && data[i] != '\n') ++i;
	}
	return true;
}

void
update_render_view(const Camera &cam)
{
	g_matrices.view = glm::lookAt(cam.pos, cam.pos + cam.front, cam.up);
	glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.matrices);
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(g_matrices.view));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void
update_render_projection(const Camera &cam, const Screen &scr)
{
	g_matrices.proj = glm::perspective(cam.fov, (float)scr.w / (float)scr.h, cam.near, cam.far);
	glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.matrices);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(g_matrices.proj));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

#define GL_CHECK_ERR(obj, ivfn, objparam, infofn, fmt, ...)\
{\
	GLint status;\
	ivfn(obj, objparam, &status);\
	if (status == GL_FALSE) {\
		GLint infolog_len;\
		ivfn(obj, GL_INFO_LOG_LENGTH, &infolog_len);\
		GLchar *infolog = new GLchar [infolog_len + 1];\
		infofn(obj, infolog_len, NULL, infolog);\
		fprintf(stderr, "Error: ");\
		fprintf(stderr, fmt, ## __VA_ARGS__);\
		fprintf(stderr, " - %s\n", infolog);\
		delete infolog;\
		exit(1);\
	}\
}

static GLuint
mk_shader(const GLenum type, const std::string &src, const char *defines)
{
	const GLuint id = glCreateShader(type);
	const char *src_strs[] = { "#version 330\n", defines, src.c_str() };
	glShaderSource(id, ARR_LEN(src_strs), src_strs, NULL);
	glCompileShader(id);

	const char *type_str;
	if (type == GL_VERTEX_SHADER)
		type_str = "vertex";
	else if (type == GL_GEOMETRY_SHADER)
		type_str = "geometry";
	else if (type == GL_FRAGMENT_SHADER)
		type_str = "fragment";
	else
		type_str = "DEFAULT";
	GL_CHECK_ERR(id, glGetShaderiv, GL_COMPILE_STATUS, glGetShaderInfoLog, "compile failure in %s shader", type_str);
	return id;
}

static GLuint
mk_program(const std::string &vert_src, const std::string &frag_src, const char *defines)
{
	GLuint vert_id = mk_shader(GL_VERTEX_SHADER, vert_src, defines);
	GLuint frag_id = mk_shader(GL_FRAGMENT_SHADER, frag_src, defines);
	GLuint program = glCreateProgram();

	glAttachShader(program, vert_id);
	glAttachShader(program, frag_id);
	glLinkProgram(program);

	GL_CHECK_ERR(program, glGetProgramiv, GL_LINK_STATUS, glGetProgramInfoLog, "linker failure");

	glDetachShader(program, vert_id);
	glDetachShader(program, frag_id);
	glDeleteShader(vert_id);
	glDeleteShader(frag_id);

	return program;
}

static SDL_Surface *
flip_surface_x(SDL_Surface* s)
{
	auto getpixel = [] (SDL_Surface *surface, int x, int y)
	{
		uint32_t *pixels = (uint32_t *)surface->pixels;
		return pixels[(y * surface->w) + x];
	};
	auto putpixel = [] (SDL_Surface *surface, int x, int y, uint32_t pixel)
	{
		uint32_t *pixels = (uint32_t *)surface->pixels;
		pixels[(y * surface->w) + x] = pixel;
	};

	SDL_Surface* flip = SDL_CreateRGBSurface(0, s->w, s->h, s->format->BitsPerPixel, s->format->Rmask, s->format->Gmask, s->format->Bmask, s->format->Amask);
	if (!flip) {
		zabort("SDL_CreateRGBSurface failed! SDL Error: %s\n", SDL_GetError());
	}
	for(int y = 0; y < s->h; ++y) {
		for(int x = 0; x < s->w; ++x) {
			//copy pixels, but reverse the y pixels
			putpixel(flip, x, y, getpixel(s, x, s->h - y - 1));
		}
	}
	return flip;
};

static SDL_Surface *
load_surface(const char *fname)
{
	SDL_Surface *s;
	if (!(s = IMG_Load(fname))) {
		zabort("IMG_Load failed! IMG_GetError: %s\n", IMG_GetError);
	}

	// opengl wants the origin on the bottom, so we flip it on the x axis
	SDL_Surface *flip = flip_surface_x(s);
	SDL_FreeSurface(s);
	return flip;
}


void
render_init(const Camera &cam, const Screen &scr)
{
	// init glew
	{
		glewExperimental = true;
		GLenum err = glewInit();
		if (GLEW_OK != err)
			zabort("%s\n", glewGetErrorString(err));
	}

	glEnable(GL_DEPTH_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glViewport(0, 0, scr.w, scr.h);

	// init shaders
	{
		auto vert = load_file("shader.vert", "r");
		assert(vert);
		auto frag = load_file("shader.frag", "r");
		assert(frag);

		g_shaders.tex = mk_program(vert.value(), frag.value(), "#define TEX\n");
		g_shaders.nolight = mk_program(vert.value(), frag.value(), "#define NO_LIGHT\n");
		g_shaders.notex = mk_program(vert.value(), frag.value(), "#define NO_TEX\n");
	}

	// load textures
	{
	  	SDL_Surface *s = load_surface("cube.png");
	  	glGenTextures(1, &g_tex);
	  	glBindTexture(GL_TEXTURE_2D, g_tex);
	  	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	  	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	  	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	  	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	  	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s->w, s->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s->pixels);
	  	glGenerateMipmap(GL_TEXTURE_2D);
	  	SDL_FreeSurface(s);
	  	glBindTexture(GL_TEXTURE_2D, 0);
	}

	auto set_bind_pt = [](const GLuint program, const char *name, const GLuint bind_pt) {
		const GLuint ind = glGetUniformBlockIndex(program, name);
		glUniformBlockBinding(program, ind, bind_pt);
	};

	// init matrix ubo
	{
		set_bind_pt(g_shaders.tex, "Matrices", 0);
		set_bind_pt(g_shaders.notex, "Matrices", 0);
		set_bind_pt(g_shaders.nolight, "Matrices", 0);
		glGenBuffers(1, &g_ubos.matrices);

		g_matrices.view = glm::lookAt(cam.pos, cam.pos + cam.front, cam.up);
		g_matrices.proj = glm::perspective(cam.fov, (float)scr.w / (float)scr.h, cam.near, cam.far);
		glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.matrices);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(Matrices), &g_matrices, GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_ubos.matrices);
	}

	// init light ubo
	{
		set_bind_pt(g_shaders.tex, "Lights", 1);
		set_bind_pt(g_shaders.notex, "Lights", 1);
		glGenBuffers(1, &g_ubos.lights);

		g_lights.dir.direction = glm::vec4(-0.2f, -1.0f, -0.3f, 0.0f);
		g_lights.dir.ambient = glm::vec4(0.05f, 0.05f, 0.05f, 0.0f);
		g_lights.dir.diffuse = glm::vec4(0.4f, 0.4f, 0.4f, 0.0f);
		g_lights.dir.specular = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);
		g_lights.spot.ambient = glm::vec4(0.05f, 0.05f, 0.05f, 0.0f);
		g_lights.spot.diffuse = glm::vec4(0.8f, 0.8f, 0.8f, 0.0f);
		g_lights.spot.specular = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
		g_lights.spot.outer_cutoff = glm::cos(glm::radians(17.5f));
		g_lights.spot.inner_cutoff = glm::cos(glm::radians(12.5f));
		g_lights.spot.constant = 1.0f;
		g_lights.spot.linear = 0.09f;
		g_lights.spot.quadratic = 0.032f;
		for (int i = 0; i < MAX_PT_LIGHTS; ++i) {
			g_lights.points[i].is_valid = false;
		}

		glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.lights);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(Lights), &g_lights, GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		glBindBufferBase(GL_UNIFORM_BUFFER, 1, g_ubos.lights);
	}

	// init material uniform
	{
		glUseProgram(g_shaders.tex);
		glUniform3f(glGetUniformLocation(g_shaders.tex, "mat.ambient"),  1.0f, 0.5f, 0.31f);
		//glUniform3f(glGetUniformLocation(g_shaders.tex, "mat.diffuse"),  1.0f, 0.5f, 0.31f);
		//glUniform3f(glGetUniformLocation(g_shaders.tex, "mat.specular"), 0.5f, 0.5f, 0.5f);
		glUniform1i(glGetUniformLocation(g_shaders.tex, "mat.diffuse"), 0);
		//glUniform1i(glGetUniformLocation(g_shaders.tex, "mat.specular"), 1);
		glUniform1f(glGetUniformLocation(g_shaders.tex, "mat.shininess"), 64.0f);
		glUseProgram(0);
	}

	// init vertex array objects
	{
		auto mk_render_data = [](const Model *m, const GLuint vao, const GLuint *buf_ids) {
			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, buf_ids[vert_buf]);
			glBufferData(GL_ARRAY_BUFFER, m->vertices.size() * sizeof(GLfloat), m->vertices.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
			glEnableVertexAttribArray(0);
	
			glBindBuffer(GL_ARRAY_BUFFER, buf_ids[norm_buf]);
			glBufferData(GL_ARRAY_BUFFER, m->normals.size() * sizeof(GLfloat), m->normals.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
			glEnableVertexAttribArray(1);
	
			if (m->tex_coords.size()) {
				glBindBuffer(GL_ARRAY_BUFFER, buf_ids[tex_buf]);
				glBufferData(GL_ARRAY_BUFFER, m->tex_coords.size() * sizeof(GLfloat), m->tex_coords.data(), GL_STATIC_DRAW);
				glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
				glEnableVertexAttribArray(2);
			}
	
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf_ids[ind_buf]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->indices.size() * sizeof(GLuint), m->indices.data(), GL_STATIC_DRAW);
	
			return Render_Data { glm::vec3(0.0f), vao, buf_ids[ind_buf], m->indices.size() };
		};

		const char *fnames[] = { "cube.ahh" };
		constexpr int num_files = sizeof(fnames) / sizeof(fnames[0]);
		constexpr int max_models = num_files;
		constexpr int max_array_objs = max_models;
		constexpr int max_buffer_objs = max_models * num_buffers;

		Model ms[max_models];
		GLuint arr_ids[max_array_objs];
		GLuint buf_ids[max_buffer_objs];
		glGenVertexArrays(max_array_objs, arr_ids);
		glGenBuffers(max_buffer_objs, buf_ids);

		for (int i = 0; i < num_files; ++i) {
			bool succ = load_model(fnames[i], &ms[i]);
			if (succ)
				g_render_groups.tex.push_back(mk_render_data(&ms[i], arr_ids[i], &buf_ids[i*num_buffers]));
		}
		glBindVertexArray(0);
	}
}

void
render(const Camera &cam)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(g_shaders.tex);
	GLint model_loc = glGetUniformLocation(g_shaders.tex, "u_model");
	glm::mat4 model;
	for (const Render_Data &rd : g_render_groups.tex) {
		model = glm::translate(model, rd.pos);
		model = glm::scale(model, glm::vec3(1.0f));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, g_tex);
		glBindVertexArray(rd.vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rd.ibo);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
		glDrawElements(GL_TRIANGLES, rd.num_indices, GL_UNSIGNED_INT, 0);
	}
	glBindVertexArray(0);
	glUseProgram(0);
}
