#include <stdio.h>
#include <GL/glew.h>
#include <assert.h>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tuple>
#include <memory>
#include <stdint.h>
#include "render.h"

struct Render_Info {
	GLuint vao;
	GLuint ibo;
	size_t num_indices;
};

static GLuint g_tex_lighting_prog, g_notex_lighting_prog, g_no_lighting_prog;
static GLuint g_tex, g_spec_tex;
static std::vector<Render_Info> g_render_group;

static void init_glew();
static void init_gl(const int screen_width, const int screen_height);
static void init_shaders();
static void init_buffers();
static void load_textures();

constexpr int max_pt_lights = 4;
bool points[max_pt_lights];

void
make_point_light(vec3f pos)
{
	// TODO: stick lighting data in a uniform buffer obj
	glUseProgram(g_tex_lighting_prog);
	for (int i = 0; i < max_pt_lights; ++i) {
		if (!points[i]) {
			points[i] = true;
			char buf[256];
			sprintf(buf, "points[%d].is_valid", i);
			glUniform1i(glGetUniformLocation(g_tex_lighting_prog, buf), true);
			sprintf(buf, "points[%d].position", i);
			glUniform3f(glGetUniformLocation(g_tex_lighting_prog, buf), pos.x, pos.y + 0.5f, pos.z);
			sprintf(buf, "points[%d].ambient", i);
			glUniform3f(glGetUniformLocation(g_tex_lighting_prog, buf), 0.05f, 0.05f, 0.05f);
			sprintf(buf, "points[%d].diffuse", i);
			glUniform3f(glGetUniformLocation(g_tex_lighting_prog, buf), 0.8f, 0.8f, 0.8f);
			sprintf(buf, "points[%d].specular", i);
			glUniform3f(glGetUniformLocation(g_tex_lighting_prog, buf), 1.0f, 1.0f, 1.0f);
			sprintf(buf, "points[%d].constant", i);
			glUniform1f(glGetUniformLocation(g_tex_lighting_prog, buf), 1.0f);
			sprintf(buf, "points[%d].linear", i);
			glUniform1f(glGetUniformLocation(g_tex_lighting_prog, buf), 0.09);
			sprintf(buf, "points[%d].quadratic", i);
			glUniform1f(glGetUniformLocation(g_tex_lighting_prog, buf), 0.032);
			glUseProgram(0);
			return;
		}
	}
	z_log_err("Max point lights reached!");
	glUseProgram(0);
}

struct Model_Info {
	std::vector<GLfloat> vertices;
	std::vector<GLfloat> tex_coords;
	std::vector<GLfloat> normals;
	std::vector<GLuint> indices;
};

// hacky obj (sort of) loader
bool
load_model_info(const char *fname, Model_Info *mi)
{
	std::optional<std::string> load = load_file(fname, "r");
	if (!load)
		return false;
	const std::string &data = load.value();

	// first pass gets counts
	size_t num_verts = 0;
	size_t num_norms = 0;
	size_t num_inds = 0;
	size_t num_texcs = 0;

	for (uint64_t i = 0; data[i]; ++i) {
		if (data[i] == 'v') {
			if (data[i+1] == 't')
				++num_texcs;
			else if (data[i+1] == 'n')
				++num_norms;
			else if (data[i+1] == ' ')
				++num_verts;
		} else if (data[i] == 'f') {
			++num_inds;
		}
	}

	mi->vertices.reserve(num_verts);
	mi->normals.reserve(num_norms);
	mi->indices.reserve(num_inds);
	mi->tex_coords.reserve(num_texcs);

	// second pass loads the data
	for (uint64_t i = 0; data[i]; ++i) {
		if (data[i] == 'v') { // vertex
			if (data[i+1] == 't') { // tex coord
				GLfloat t1, t2;
				if (sscanf(&data[i+2], "%f %f", &t1, &t2) < 2) {
					z_log_err("obj parser error in file %s\n", fname);
					return false;
				}
				mi->tex_coords.insert(mi->tex_coords.end(), { t1, t2 });
			} else {
				GLfloat v1, v2, v3;
				if (sscanf(&data[i+2], "%f %f %f", &v1, &v2, &v3) < 3) {
					z_log_err("obj parser error in file %s\n", fname);
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
				z_log_err("obj parser error in file %s\n", fname);
				return false;
			}
			mi->indices.insert(mi->indices.end(), { i1, i2, i3 });
		}
		while (data[i] && data[i] != '\n') ++i;
	}
	return true;
}

void
init_uniforms(const int screenw, const int screenh)
{
	glUseProgram(g_tex_lighting_prog);
	// mat
	glUniform3f(glGetUniformLocation(g_tex_lighting_prog, "mat.ambient"),  1.0f, 0.5f, 0.31f);
	//glUniform3f(glGetUniformLocation(g_tex_lighting_prog, "mat.diffuse"),  1.0f, 0.5f, 0.31f);
	//glUniform3f(glGetUniformLocation(g_tex_lighting_prog, "mat.specular"), 0.5f, 0.5f, 0.5f);
	glUniform1i(glGetUniformLocation(g_tex_lighting_prog, "mat.diffuse"), 0);
	//glUniform1i(glGetUniformLocation(g_tex_lighting_prog, "mat.specular"), 1);
	glUniform1f(glGetUniformLocation(g_tex_lighting_prog, "mat.shininess"), 64.0f);

	// dir lights
	glUniform3f(glGetUniformLocation(g_tex_lighting_prog, "dir.direction"), -0.2f, -1.0f, -0.3f);
	glUniform3f(glGetUniformLocation(g_tex_lighting_prog, "dir.ambient"), 0.05f, 0.05f, 0.05f);
	glUniform3f(glGetUniformLocation(g_tex_lighting_prog, "dir.diffuse"), 0.4f, 0.4f, 0.4f);
	glUniform3f(glGetUniformLocation(g_tex_lighting_prog, "dir.specular"), 0.5f, 0.5f, 0.5f);

	// pt lights
	for (int i = 0; i < max_pt_lights; ++i) {
		char buf[256];
		sprintf(buf, "points[%d].is_valid", i);
		glUniform1i(glGetUniformLocation(g_tex_lighting_prog, buf), false);
		points[i] = false;
	}

	// spot light
	glUniform3f(glGetUniformLocation(g_tex_lighting_prog, "spot.ambient"), 0.05f, 0.05f, 0.05f);
	glUniform3f(glGetUniformLocation(g_tex_lighting_prog, "spot.diffuse"), 0.8f, 0.8f, 0.8f);
	glUniform3f(glGetUniformLocation(g_tex_lighting_prog, "spot.specular"), 1.0f, 1.0f, 1.0f);
	glUniform1f(glGetUniformLocation(g_tex_lighting_prog, "spot.constant"), 1.0f);
	glUniform1f(glGetUniformLocation(g_tex_lighting_prog, "spot.linear"), 0.09);
	glUniform1f(glGetUniformLocation(g_tex_lighting_prog, "spot.quadratic"), 0.032);
	glUniform1f(glGetUniformLocation(g_tex_lighting_prog, "spot.inner_cutoff"), glm::cos(glm::radians(12.5f)));
	glUniform1f(glGetUniformLocation(g_tex_lighting_prog, "spot.outer_cutoff"), glm::cos(glm::radians(17.5f)));

	glUseProgram(0);
}

void
render_init(const int screenw, const int screenh)
{
	init_gl(screenw, screenh);
	load_textures();
	init_uniforms(screenw, screenh);
}

void
render(Camera cam)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(g_tex_lighting_prog);
	glm::mat4 view = glm::lookAt(cam.pos, cam.pos + cam.front, cam.up);
	glm::mat4 projection = glm::perspective(45.0f, (GLfloat)800 / (GLfloat)600, 0.1f, 100.0f);
	glUniformMatrix4fv(glGetUniformLocation(g_tex_lighting_prog, "u_view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(g_tex_lighting_prog, "u_projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniform3f(glGetUniformLocation(g_tex_lighting_prog, "u_view_pos"), cam.pos.x, cam.pos.y, cam.pos.z);
	GLint model_loc = glGetUniformLocation(g_tex_lighting_prog, "u_model");
	glm::mat4 model;
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(1.0f));
	for (const Render_Info &ri : g_render_group) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, g_tex);
		glBindVertexArray(ri.vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ri.ibo);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
		glDrawElements(GL_TRIANGLES, ri.num_indices, GL_UNSIGNED_INT, 0);
	}
	glBindVertexArray(0);
	glUseProgram(0);
}

static void
init_gl(const int screen_width, const int screen_height)
{
	init_glew();
	init_shaders();
	init_buffers();

	glEnable(GL_DEPTH_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glViewport(0, 0, screen_width, screen_height);
}

static void
init_glew()
{
	glewExperimental = true;
	GLenum err = glewInit();
	if (GLEW_OK != err)
		z_abort("%s\n", glewGetErrorString(err));
	fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
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
mk_shader(const GLenum type, const std::string &src)
{
	const GLuint id = glCreateShader(type);
	const char *c_str = src.c_str();
	glShaderSource(id, 1, &c_str, NULL);
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
mk_program(const std::string &vert_src, const std::string &frag_src)
{
	GLuint vert_id = mk_shader(GL_VERTEX_SHADER, vert_src);
	GLuint frag_id = mk_shader(GL_FRAGMENT_SHADER, frag_src);
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

static void
init_shaders()
{
	auto tl_vert = load_file("tex_lighting.vert", "r");
	assert(tl_vert);
	auto tl_frag = load_file("tex_lighting.frag", "r");
	assert(tl_frag);
	auto ntl_vert = load_file("notex_lighting.vert", "r");
	assert(ntl_vert);
	auto ntl_frag = load_file("notex_lighting.frag", "r");
	assert(ntl_frag);
	auto nl_vert = load_file("no_lighting.vert", "r");
	assert(nl_vert);
	auto nl_frag = load_file("no_lighting.frag", "r");
	assert(nl_frag);

	g_tex_lighting_prog = mk_program(tl_vert.value(), tl_frag.value());
	g_no_lighting_prog = mk_program(nl_vert.value(), nl_frag.value());
	g_notex_lighting_prog = mk_program(ntl_vert.value(), ntl_frag.value());
}

enum GL_Buffers {
	vert_buf = 0,
	norm_buf,
	ind_buf,
	tex_buf,
	num_buffers
};

static void
init_buffers()
{
	auto mk_render_info = [](const Model_Info *mi, const GLuint vao, const GLuint *buf_ids) {
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, buf_ids[vert_buf]);
		glBufferData(GL_ARRAY_BUFFER, mi->vertices.size() * sizeof(GLfloat), mi->vertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, buf_ids[norm_buf]);
		glBufferData(GL_ARRAY_BUFFER, mi->normals.size() * sizeof(GLfloat), mi->normals.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
		glEnableVertexAttribArray(1);

		if (mi->tex_coords.size()) {
			glBindBuffer(GL_ARRAY_BUFFER, buf_ids[tex_buf]);
			glBufferData(GL_ARRAY_BUFFER, mi->tex_coords.size() * sizeof(GLfloat), mi->tex_coords.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
			glEnableVertexAttribArray(2);
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf_ids[ind_buf]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mi->indices.size() * sizeof(GLuint), mi->indices.data(), GL_STATIC_DRAW);

		return Render_Info { vao, buf_ids[ind_buf], mi->indices.size() };
	};

	const char *fnames[] = { "cube.fmt" };
	constexpr size_t num_files = sizeof(fnames) / sizeof(fnames[0]);
	constexpr size_t max_models = num_files;
	constexpr size_t max_array_objs = max_models;
	constexpr size_t max_buffer_objs = max_models * num_buffers;

	Model_Info mis[max_models];
	GLuint arr_ids[max_array_objs];
	GLuint buf_ids[max_buffer_objs];
	glGenVertexArrays(max_array_objs, arr_ids);
	glGenBuffers(max_buffer_objs, buf_ids);

	for (int i = 0; i < num_files; ++i) {
		bool succ = load_model_info(fnames[i], &mis[i]);
		if (succ)
			g_render_group.push_back(mk_render_info(&mis[i], arr_ids[i], &buf_ids[i*num_buffers]));
	}
	glBindVertexArray(0);
}

SDL_Surface *
flip_surface_x (SDL_Surface* s)
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
		z_abort("SDL_CreateRGBSurface failed! SDL Error: %s\n", SDL_GetError());
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
	SDL_Surface *surface;
	if (!(surface = IMG_Load(fname))) {
		z_abort("IMG_Load failed! IMG_GetError: %s\n", IMG_GetError);
	}

	//opengl wants the origin on the bottom, so we flip it on the x axis
	SDL_Surface *flip = flip_surface_x(surface);
	SDL_FreeSurface(surface);
	return flip;
}

static void
load_textures()
{
	SDL_Surface *surface = load_surface("cube.png");

	glGenTextures(1, &g_tex);
	glBindTexture(GL_TEXTURE_2D, g_tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	SDL_FreeSurface(surface);
//
//	surface = load_surface("container2_specular.png");
//
//	glGenTextures(1, &g_spec_tex);
//	glBindTexture(GL_TEXTURE_2D, g_spec_tex);
//
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
//	glGenerateMipmap(GL_TEXTURE_2D);
//	SDL_FreeSurface(surface);

	glBindTexture(GL_TEXTURE_2D, 0);
}
