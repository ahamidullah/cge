#include <stdio.h>
#include <GL/glew.h>
#include <assert.h>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#define GLM_SWIZZLE
#include <glm/gtc/type_ptr.hpp>
#include <stdint.h>
#include <stddef.h>
#include "render.h"

constexpr int MAX_PT_LIGHTS = 4;

int
temp_str_hash(const char *key)
{
	unsigned sum = 0;
	for (unsigned i = 0; i < strlen(key); ++i) {
		sum += key[i];
	}
	return sum % 100;
}

struct Instance {
	glm::vec3 pos;
	glm::vec3 rot;
	GLfloat scale;
};

struct Model_Group {
	GLuint vao;
	GLuint ibo;
	GLuint num_indices;
	Array<Instance> instances;
};

struct Render_Group {
	GLuint shader;
	Array<Model_Group> models;
};

enum Shader_Enum {
	tex_sh = 0,
	notex_sh,
	nolight_sh,
	num_shaders
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

// cpu copy of frag shader ubo light structures (matching alignment)
struct Lights {
	alignas(16) Dir_Light dir;
	alignas(16) Spot_Light spot;
	alignas(16) Point_Light points[MAX_PT_LIGHTS];
};

struct Ubo_Ids {
	GLuint matrices;
	GLuint lights;
};

struct Matrices {
	glm::mat4 proj;
	glm::mat4 view;
};

enum GL_Buffers {
	vert_buf = 0,
	norm_buf,
	ind_buf,
	tex_buf,
	num_glbufs
};

struct Raw_Model {
	char *tex_fname;
	GLfloat *vertices;
	GLfloat *tex_coords;
	GLfloat *normals;
	GLuint *indices;
	size_t num_inds;
	size_t num_verts;
	size_t num_tcoords;
	size_t num_norms;
};

static Hash_Table<ModelID, int (*)(const char *)> g_model_table;
static Render_Group g_rgroups[num_shaders];
static Lights g_lights;
static Matrices g_matrices;
static Ubo_Ids g_ubos;
static GLuint g_tex, g_spec_tex;
constexpr int nm = 0;

std::optional<glm::vec3>
raycast_plane(const glm::vec2 &screen_ray, const glm::vec3 &plane_normal, const glm::vec3 &origin, const float origin_ofs, const Screen &scr)
{
	float x = (2.0f * screen_ray.x) / scr.w - 1.0f;
	float y = 1.0f - (2.0f * screen_ray.y) / scr.h;
	glm::vec4 clip_ray = glm::vec4(x, y, -1.0f, 1.0f);
	glm::vec4 eye_ray = glm::inverse(g_matrices.proj) * clip_ray;
	eye_ray = glm::vec4(eye_ray.x, eye_ray.y, -1.0f, 0.0f);
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
	for (int i = 0; i < MAX_PT_LIGHTS; ++i) {
		if (!g_lights.points[i].is_valid) {
			g_lights.points[i].is_valid = true;
			g_lights.points[i].position = glm::vec4(pos, 0.0f);
			g_lights.points[i].ambient = glm::vec4(0.05f, 0.05f, 0.05f, 0.0f);
			g_lights.points[i].diffuse = glm::vec4(0.8f, 0.8f, 0.8f, 0.0f);
			g_lights.points[i].specular = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
			g_lights.points[i].constant = 1.0f;
			g_lights.points[i].linear = 0.09f;
			g_lights.points[i].quadratic = 0.032f;
			glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.lights);
			// just write out the entire point light array for now
			glBufferSubData(GL_UNIFORM_BUFFER, offsetof(Lights, points), sizeof(g_lights.points), &g_lights.points);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
			return;
		}
	}
	zerror("Max point lights reached!");
}

// obj (sort of) loader
static void
load_raw_models(const char **names, Raw_Model *rms, bool *succs, int num)
{
	const char *ext = ".ahh";
	for (int i = 0; i < num; ++i) {
		char fname[256]; // temp
		strcpy(fname, names[i]);
		strcat(fname, ext);
		char *data = load_file(fname, "r");
		if (!data) {
			succs[i] = false;
			continue;
		}
		size_t total_verts = 0;
		size_t total_norms = 0;
		size_t total_inds = 0;
		size_t total_tcoords = 0;
		for (int j = 0; data[j]; ++j) {
			if (data[j] == 'v') {
				if (data[j+1] == 't')
					total_tcoords += 2;
				else if (data[j+1] == 'n')
					total_norms += 3;
				else if (data[j+1] == ' ')
					total_verts += 3;
			} else if (data[j] == 'f') {
				total_inds += 3;
			}
			while (data[j] && data[j] != '\n') ++j;
		}
	
		rms[i].vertices = (GLfloat *)malloc(sizeof(GLfloat)*(total_verts));
		rms[i].num_verts = 0;
		rms[i].normals = (GLfloat *)malloc(sizeof(GLfloat)*(total_norms));
		rms[i].num_norms = 0;
		rms[i].indices = (GLuint *)malloc(sizeof(GLuint)*(total_inds));
		rms[i].num_inds = 0;
		rms[i].tex_coords = (GLfloat *)malloc(sizeof(GLfloat)*(total_tcoords));
		rms[i].num_tcoords = 0;
		rms[i].tex_fname = NULL;
	
		for (int j = 0; data[j]; ++j) {
			if (data[j] == 't' && data[j+1] == 'f') {
				rms[i].tex_fname = (char *)malloc(256); // temp
				// doesn't work if texture filename has spaces...
				if (sscanf(&data[j], "tf %s", rms[i].tex_fname) < 1)
					zerror("obj error: could not parse texture file name in file %s\n", fname);
			} else if (data[j] == 'v') {
				if (data[j+1] == 't') {
					GLfloat t1, t2;
					if (sscanf(&data[j+2], "%f %f", &t1, &t2) == 2) {
						rms[i].tex_coords[rms[i].num_tcoords++] = t1;
						rms[i].tex_coords[rms[i].num_tcoords++] = t2;
					} else
						zerror("parser error in %s around %d -- expected tex coords\n", fname, j);
				} else if (data[j+1] == 'n') {
					GLfloat n1, n2, n3;
					if (sscanf(&data[j+2], "%f %f %f", &n1, &n2, &n3) == 3) {
						rms[i].normals[rms[i].num_norms++] = n1;
						rms[i].normals[rms[i].num_norms++] = n2;
						rms[i].normals[rms[i].num_norms++] = n3;
					} else
						zerror("parser error in %s around %d -- expected normals\n", fname, j);
				} else if (data[j+1] == ' ') {
					GLfloat v1, v2, v3;
					if (sscanf(&data[j+2], "%f %f %f", &v1, &v2, &v3) == 3) {
						rms[i].vertices[rms[i].num_verts++] = v1;
						rms[i].vertices[rms[i].num_verts++] = v2;
						rms[i].vertices[rms[i].num_verts++] = v3;
					} else
						zerror("parser error in %s around %d -- expected vertices\n", fname, j);
				} else
					zerror("parser error in %s around %d -- syntax error\n", fname, j);
			} else if (data[j] == 'f') {
				GLuint i1, i2, i3;
				if (sscanf(&data[j+1], "%u %u %u", &i1, &i2, &i3) == 3) {
					rms[i].indices[rms[i].num_inds++] = i1;
					rms[i].indices[rms[i].num_inds++] = i2;
					rms[i].indices[rms[i].num_inds++] = i3;
				} else
					zerror("parser error in %s around %d -- expected indices\n", fname, j);
			} else
					zerror("parser error in %s around %d -- syntax error\n", fname, j);
			while (data[j] && data[j] != '\n') ++j;
		}
		free(data);
		succs[i] = true;
	}
}

static void
free_raw_models(Raw_Model *rms, int num)
{
	for (int i = 0; i < num; ++i) {
		free(rms[i].tex_fname);
		free(rms[i].vertices);
		free(rms[i].indices);
		free(rms[i].tex_coords);
		free(rms[i].normals);
	}
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
mk_shader(const GLenum type, const char *src, const char *defines)
{
	const GLuint id = glCreateShader(type);
	const char *src_strs[] = { "#version 330\n", defines, src };
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
mk_program(const char *vert_src, const char *frag_src, const char *defines)
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
load_surface(const char *fname)
{
	SDL_Surface *s;
	if (!(s = IMG_Load(fname)))
		zabort("IMG_Load failed! IMG_GetError: %s\n", IMG_GetError);
	// opengl wants the origin at the bottom, so we flip it around the x axis
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
	if (!flip)
		zabort("SDL_CreateRGBSurface failed! SDL Error: %s\n", SDL_GetError());
	for(int y = 0; y < s->h; ++y) {
		for(int x = 0; x < s->w; ++x) {
			// reverse the y pixels
			putpixel(flip, x, y, getpixel(s, x, s->h - y - 1));
		}
	}
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
		char *vert = load_file("shader.vert", "r");
		assert(vert);
		char *frag = load_file("shader.frag", "r");
		assert(frag);

		g_rgroups[tex_sh].shader = mk_program(vert, frag, "#define TEX\n");
		g_rgroups[notex_sh].shader = mk_program(vert, frag, "#define NO_TEX\n");
		g_rgroups[nolight_sh].shader = mk_program(vert, frag, "#define NO_LIGHT\n");
		free(vert);
		free(frag);
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
		set_bind_pt(g_rgroups[tex_sh].shader, "Matrices", 0);
		set_bind_pt(g_rgroups[notex_sh].shader, "Matrices", 0);
		set_bind_pt(g_rgroups[nolight_sh].shader, "Matrices", 0);
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
		set_bind_pt(g_rgroups[tex_sh].shader, "Lights", 1);
		set_bind_pt(g_rgroups[notex_sh].shader, "Lights", 1);
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
		glUseProgram(g_rgroups[tex_sh].shader);
		glUniform3f(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.ambient"),  1.0f, 0.5f, 0.31f);
		//glUniform3f(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.diffuse"),  1.0f, 0.5f, 0.31f);
		//glUniform3f(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.specular"), 0.5f, 0.5f, 0.5f);
		glUniform1i(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.diffuse"), 0);
		//glUniform1i(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.specular"), 1);
		glUniform1f(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.shininess"), 64.0f);
		glUseProgram(0);
	}

	// load models
	{
		arr_init(&g_rgroups[tex_sh].models);
		arr_init(&g_rgroups[notex_sh].models);
		arr_init(&g_rgroups[nolight_sh].models);
		hsh_init(&g_model_table, temp_str_hash);

		const char *mnames[] = { "cube" };
		constexpr int num_models = sizeof(mnames)/sizeof(mnames[0]);
		Raw_Model rawms[num_models];
		bool succs[num_models];
		load_raw_models(mnames, rawms, succs, num_models);

		GLuint vao_ids[num_models];
		GLuint buf_ids[num_models*num_glbufs];
		glGenVertexArrays(num_models, vao_ids);
		glGenBuffers(num_models*num_glbufs, buf_ids);

		for (int i = 0; i < num_models; ++i) {
			if (!succs[i]) {
				zerror("failed to load model %s", mnames[i]);
				continue;
			}
			GLuint cur_vao = vao_ids[i];
			GLuint *cur_bufs = &buf_ids[i*num_glbufs];
			Raw_Model *rm = &rawms[i];
			glBindVertexArray(cur_vao);
			glBindBuffer(GL_ARRAY_BUFFER, cur_bufs[vert_buf]);
			glBufferData(GL_ARRAY_BUFFER, rm->num_verts*sizeof(GLfloat), rm->vertices, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ARRAY_BUFFER, cur_bufs[norm_buf]);
			glBufferData(GL_ARRAY_BUFFER, rm->num_norms*sizeof(GLfloat), rm->normals, GL_STATIC_DRAW);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
			glEnableVertexAttribArray(1);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cur_bufs[ind_buf]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, rm->num_inds*sizeof(GLuint), rm->indices, GL_STATIC_DRAW);

			if (rm->tex_fname) {
				glBindBuffer(GL_ARRAY_BUFFER, cur_bufs[tex_buf]);
				glBufferData(GL_ARRAY_BUFFER, rm->num_tcoords*sizeof(GLfloat), rm->tex_coords, GL_STATIC_DRAW);
				glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
				glEnableVertexAttribArray(2);
			}
			int shader = rm->tex_fname ? tex_sh : notex_sh;
			Model_Group *mg = arr_alloc(&g_rgroups[shader].models);
			mg->vao = cur_vao;
			mg->ibo = cur_bufs[ind_buf];
			mg->num_indices = rm->num_inds;
			int model_id = arr_getid(mg);
			int rgroup_id = tex_sh;
			hsh_add(&g_model_table, mnames[i], {rgroup_id, model_id});
			arr_init(&mg->instances);
		}
		glBindVertexArray(0);
		free_raw_models(rawms, num_models);
	}
}

void
render_update_view(const Camera &cam)
{
	g_matrices.view = glm::lookAt(cam.pos, cam.pos + cam.front, cam.up);
	glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.matrices);
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(g_matrices.view));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void
render_update_projection(const Camera &cam, const Screen &scr)
{
	g_matrices.proj = glm::perspective(cam.fov, (float)scr.w / (float)scr.h, cam.near, cam.far);
	glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.matrices);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(g_matrices.proj));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

RenderID
render_add_instance(const char *name, const glm::vec3 &pos)
{
	ModelID mid = hsh_get(g_model_table, name);
	Model_Group *mg = arr_get(g_rgroups[mid.rgroup_id].models, mid.mgroup_id);
	Instance *in = arr_alloc(&mg->instances);
	in->pos = pos;
	return { mid.rgroup_id, mid.mgroup_id, arr_getid(in) };
}

void
render_update_instance(RenderID rid, const glm::vec3 &pos)
{
	Model_Group *mg = arr_get(g_rgroups[rid.rgroup_id].models, rid.mgroup_id);
	Instance *in = arr_get(mg->instances, rid.instance_id);
	in->pos = pos;
}

void
render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glm::mat4 model;
	for (int i = 0; i < num_shaders; ++i) {
		glUseProgram(g_rgroups[i].shader);
		GLint model_loc = glGetUniformLocation(g_rgroups[i].shader, "u_model");
		for (Model_Group *m = arr_first(g_rgroups[i].models); m; m = arr_next(g_rgroups[i].models, m)) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, g_tex);
			glBindVertexArray(m->vao);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);
			for (Instance *in = arr_first(m->instances); in; in = arr_next(m->instances, in)) {
				model = glm::translate(model, in->pos);
//				model = glm::scale(model, in->scale);
//				model = glm::rotate(model, 50.0f, glm::vec3(0, 1, 1));
				glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
				glDrawElements(GL_TRIANGLES, m->num_indices, GL_UNSIGNED_INT, 0);
			}
		}
	}
	glBindVertexArray(0);
	glUseProgram(0);
}

void
render_quit()
{
	// TODO: missing gldeletebuffers
	glBindVertexArray(0);
	glUseProgram(0);
	glDeleteTextures(1, &g_tex);
	for (int i = 0; i < num_shaders; ++i) {
		glDeleteProgram(g_rgroups[i].shader);
		for (Model_Group *m = arr_first(g_rgroups[i].models); m; m = arr_next(g_rgroups[i].models, m)) {
			glDeleteVertexArrays(1, &m->vao);
			arr_destroy(m->instances);
		}
		arr_destroy(g_rgroups[i].models);
	}
}

