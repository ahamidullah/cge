#include <stdio.h>
#include <GL/glew.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#define GLM_SWIZZLE
#include <glm/gtc/type_ptr.hpp>
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
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

struct Tex_Mat {
	GLuint diffuse_id;
	GLuint spec_id;
	float shininess;
};
/*
struct Notex_Mat {
	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;
	float shininess;
};
*/

union Material {
	Tex_Mat tex;
};

struct Mesh_Info {
	GLuint num_indices;
	Material mat;
};

struct Model {
	GLuint vao;
	GLuint ebo;
	int num_meshes;
	Mesh_Info meshes[256]; // temp
	Array<glm::mat4> instances;
};

struct Render_Group {
	GLuint shader;
	int num_models;
	Model models[256]; // temp
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

struct Raw_Mesh_Info {
	GLuint num_indices;
	char diffuse_fname[256]; //temp
};

struct Raw_Model {
	std::vector<Raw_Mesh_Info> raw_mesh_infos;
	std::vector<GLfloat> positions;
	std::vector<GLfloat> uvs;
	std::vector<GLfloat> normals;
	std::vector<GLuint> indices;
};

typedef int (*String_Hash)(const char *);

static Hash_Table<ModelID, String_Hash> g_model_table;
static Hash_Table<GLuint, String_Hash> g_tex_table;
static Render_Group g_rgroups[num_shaders];
static Lights g_lights;
static Matrices g_matrices;
static Ubo_Ids g_ubos;
//static GLuint g_tex, g_spec_tex;
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

static void
process_node(aiNode* node, const aiScene* scene, Raw_Model *rm)
{
	for (int m = 0; m < node->mNumMeshes; ++m) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[m]];
		for (int i = 0; i < mesh->mNumVertices; ++i) {
			rm->positions.push_back(mesh->mVertices[i].x);
			rm->positions.push_back(mesh->mVertices[i].y);
			rm->positions.push_back(mesh->mVertices[i].z);
		}
		for (int i = 0; i < mesh->mNumVertices; ++i) {
			rm->normals.push_back(mesh->mNormals[i].x);
			rm->normals.push_back(mesh->mNormals[i].y);
			rm->normals.push_back(mesh->mNormals[i].z);
		}
		int num_mesh_inds = 0;
		for (int i = 0; i < mesh->mNumFaces; ++i) {
			aiFace face = mesh->mFaces[i];
			for(GLuint j = 0; j < face.mNumIndices; j++) {
				rm->indices.push_back(face.mIndices[j]);
				++num_mesh_inds;
			}
		}
		if (mesh->mTextureCoords[0]) {
			for(int i = 0; i < mesh->mNumVertices; ++i) {
				rm->uvs.push_back(mesh->mTextureCoords[0][i].x);
				rm->uvs.push_back(mesh->mTextureCoords[0][i].y);
			}
		}
		if (mesh->mMaterialIndex >= 0) {
			aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
			aiString str;
			mat->GetTexture(aiTextureType_DIFFUSE, 0, &str);
			rm->raw_mesh_infos.resize(rm->raw_mesh_infos.size()+1); // vector garbage
			rm->raw_mesh_infos.back().num_indices = num_mesh_inds;
			strcpy(rm->raw_mesh_infos.back().diffuse_fname, str.C_Str());
//			for (int i = 0; i < mat->GetTextureCount(aiTextureType_DIFFUSE); i++) {
//				aiString str;
//			}
		}
	}
	for(int i = 0; i < node->mNumChildren; ++i)
		process_node(node->mChildren[i], scene, rm);
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

/*
static int
load_surfaces(char (*fnames)[256], int n, SDL_Surface **ret)
{
	auto getpixel = [] (SDL_Surface *surface, int x, int y)
	{
		return ((uint32_t *)surface->pixels)[(y * surface->w) + x];
	};
	auto putpixel = [] (SDL_Surface *surface, int x, int y, uint32_t pixel)
	{
		((uint32_t *)surface->pixels)[(y * surface->w) + x] = pixel;
	};
	SDL_Surface *origs[n];
	int succs = 0;
	for (int i = 0; i < n; ++i) {
		if (!(origs[i] = IMG_Load(fnames[i]))) {
			zabort("IMG_Load failed. IMG_GetError: %s\n", IMG_GetError());
			continue;
		}
		// opengl wants the origin at the bottom, so we flip it around the x axis
		ret[i] = SDL_CreateRGBSurface(0, origs[i]->w, origs[i]->h, origs[i]->format->BitsPerPixel, origs[i]->format->Rmask, origs[i]->format->Gmask, origs[i]->format->Bmask, origs[i]->format->Amask);
		if (!ret[i]) {
			zabort("SDL_CreateRGBSurface failed! SDL Error: %s\n", SDL_GetError());
			continue;
		}
		for(int y = 0; y < origs[i]->h; ++y) {
			for(int x = 0; x < origs[i]->w; ++x) {
				// reverse the y pixels
				putpixel(ret[i], x, y, getpixel(origs[i], x, origs[i]->h - y - 1));
			}
		}
		printf("Loaded image %s\n", fnames[i]);
		SDL_FreeSurface(origs[i]);
		++succs;
	}
	return succs;
}

static void
free_surfaces(SDL_Surface **surfs, int n)
{
	for (int i = 0; i < n; ++i) {
		SDL_FreeSurface(surfs[i]);
	}
}

*/
/*
	auto get_pixel = [] (SDL_Surface *surface, int x, int y)
	{
		return ((uint32_t *)surface->pixels)[(y * surface->w) + x];
	};
	auto put_pixel = [] (SDL_Surface *surface, int x, int y, uint32_t pixel)
	{
		((uint32_t *)surface->pixels)[(y * surface->w) + x] = pixel;
	};
*/

#define GET_PIXEL(s, x, y) (((uint32_t *)s->pixels)[(y * s->w) + x])
#define PUT_PIXEL(s, x, y, p) (((uint32_t *)s->pixels)[(y * s->w) + x] = p)
void
swap_pixels (SDL_Surface *s, int x1, int y1, int x2, int y2)
{
	uint32_t temp=GET_PIXEL(s, x1, y1);
	PUT_PIXEL(s, x1, y1, GET_PIXEL(s, x2, y2));
	PUT_PIXEL(s, x2, y2, temp);
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
		char *vert = load_file("shaders/shader.vert", "r");
		assert(vert);
		char *frag = load_file("shaders/shader.frag", "r");
		assert(frag);

		g_rgroups[tex_sh].shader = mk_program(vert, frag, "#define TEX\n");
		g_rgroups[notex_sh].shader = mk_program(vert, frag, "#define NO_TEX\n");
		g_rgroups[nolight_sh].shader = mk_program(vert, frag, "#define NO_LIGHT\n");
		free(vert);
		free(frag);
	}

	// load textures
	{
/*		SDL_Surface *s[1];
		const char *fnames[1] = { "cube.png" };
	  	load_surfaces(fnames, 1, s);
	  	glGenTextures(1, &g_tex);
		glActiveTexture(GL_TEXTURE0);
	  	glBindTexture(GL_TEXTURE_2D, g_tex);
	  	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	  	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	  	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	  	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	  	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s[0]->w, s[0]->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s[0]->pixels);
	  	glGenerateMipmap(GL_TEXTURE_2D);
	  	SDL_FreeSurface(s[0]);
	  	glBindTexture(GL_TEXTURE_2D, 0);*/
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
//		glUseProgram(g_rgroups[tex_sh].shader);
//		glUniform3f(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.ambient"),  1.0f, 0.5f, 0.31f);
		//glUniform3f(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.diffuse"),  1.0f, 0.5f, 0.31f);
		glUniform3f(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.specular"), 0.5f, 0.5f, 0.5f);
		glUniform1i(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.diffuse"), 0);
		//glUniform1i(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.specular"), 1);
		glUniform1f(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.shininess"), 64.0f);
//		glUseProgram(0);
	}

	// load textures
	{
		hsh_init(&g_tex_table, temp_str_hash);
		char paths[256][256]; // temp
		int num_files = get_paths("textures/", paths);
		SDL_Surface *texs[256]; // temp
		char *succ_load_paths[256]; // temp
		int num_texs = 0;
		for (int i = 0; i < num_files; ++i) {
			if (!(texs[num_texs] = IMG_Load(paths[i]))) {
				zabort("IMG_Load failed! IMG_GetError: %s\n", IMG_GetError);
				continue; // reuse the sdl_surface
			}
			succ_load_paths[num_texs] = paths[i];
			printf("Loaded texture file %s\n", paths[i]);
			++num_texs;
		}
		// opengl wants the origin at the bottom, so we flip it around the x axis
//		for (int i = 0; i < num_texs; ++i) {
//			for(int y = 0; y < texs[i]->h; ++y)
//				for(int x = 0; x < texs[i]->w; ++x)
//					swap_pixels(texs[i], x, y, x, texs[i]->h - y - 1);
//		}
		GLuint tex_ids[256]; // temp
		glGenTextures(num_texs, tex_ids);
		glActiveTexture(GL_TEXTURE0);
		for (int i = 0; i < num_texs; ++i) {
			GLint format = texs[i]->format->BytesPerPixel == 4 ? GL_RGBA : GL_RGB;
	  		glBindTexture(GL_TEXTURE_2D, tex_ids[i]);
	  		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	  		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	  		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	  		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	  		glTexImage2D(GL_TEXTURE_2D, 0, format, texs[i]->w, texs[i]->h, 0, format, GL_UNSIGNED_BYTE, texs[i]->pixels);
	  		glGenerateMipmap(GL_TEXTURE_2D);
		}
		hsh_add_batch(&g_tex_table, succ_load_paths, tex_ids, num_texs);
	  	glBindTexture(GL_TEXTURE_2D, 0);
		for (int i = 0; i < num_texs; ++i)
			SDL_FreeSurface(texs[i]);
	}

	// load models
	{
		hsh_init(&g_model_table, temp_str_hash);

		char paths[256][256]; // temp
		int num_files = get_paths("models/", paths);
		Raw_Model rawms[256]; // temp
		for (int i = 0; i < 1; ++i) {
			Assimp::Importer import;
			const aiScene* scene = import.ReadFile("nanosuit.fbx", aiProcess_Triangulate | aiProcess_FlipUVs);
			if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
				zerror("Assimp error: %s", import.GetErrorString());
				continue;
			}
			process_node(scene->mRootNode, scene, &rawms[i]);
		}

		// TEMP
		GLuint vao_ids[256];
		GLuint buf_ids[256*num_glbufs];

		glGenVertexArrays(256, vao_ids);
		glGenBuffers(256*num_glbufs, buf_ids);

		for (int i = 0; i < 1; ++i) {
			GLuint cur_vao = vao_ids[i];
			GLuint *cur_bufs = &buf_ids[i*num_glbufs];
			Raw_Model *rm = &rawms[i];

			glBindVertexArray(cur_vao);
			glBindBuffer(GL_ARRAY_BUFFER, cur_bufs[vert_buf]);
			glBufferData(GL_ARRAY_BUFFER, rm->positions.size()*sizeof(GLfloat), rm->positions.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ARRAY_BUFFER, cur_bufs[norm_buf]);
			glBufferData(GL_ARRAY_BUFFER, rm->normals.size()*sizeof(GLfloat), rm->normals.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
			glEnableVertexAttribArray(1);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cur_bufs[ind_buf]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, rm->indices.size()*sizeof(GLuint), rm->indices.data(), GL_STATIC_DRAW);

//			if (rm->tex_fname) {
				glBindBuffer(GL_ARRAY_BUFFER, cur_bufs[tex_buf]);
				glBufferData(GL_ARRAY_BUFFER, rm->uvs.size()*sizeof(GLfloat), rm->uvs.data(), GL_STATIC_DRAW);
				glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
				glEnableVertexAttribArray(2);
//			}
			printf("Loaded model file %s\n", paths[i]);
//			int shader = rm->tex_fname ? tex_sh : notex_sh;
			int model_ind = g_rgroups[tex_sh].num_models++;
			Model *m = &g_rgroups[tex_sh].models[model_ind];
			m->vao = cur_vao;
			m->ebo = cur_bufs[ind_buf];
			m->num_meshes = rm->raw_mesh_infos.size();
			// get base model name without path and extension
			char *base_mname = paths[i];
			printf("base name %s\n", base_mname);
			while (*base_mname != '\0' && *base_mname != '.')
				++base_mname;
			*base_mname = '\0';
			printf("base name %s\n", base_mname);
			while (*(base_mname-1) != '/')
				--base_mname;
			printf("base name %s\n", base_mname);
			hsh_add(&g_model_table, base_mname, {tex_sh, model_ind});
			for (int j = 0; j < m->num_meshes; ++j) {
				m->meshes[j].mat.tex.diffuse_id = *(hsh_get(g_tex_table, rm->raw_mesh_infos[j].diffuse_fname));
				m->meshes[j].num_indices = rm->raw_mesh_infos[j].num_indices;
			}
			arr_init(&m->instances);
		}

		glBindVertexArray(0);
	//	free_raw_models(rawms, num_models);
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
	std::optional<ModelID> mid = hsh_get(g_model_table, name);
	if (!mid) { // DEBUG
		zerror("tried to add instance of invalid model '%s'", name);
		return RENDER_ID_ERR;
	}
	Model *m = &g_rgroups[mid->rgroup_id].models[mid->mgroup_id];
	glm::mat4 *in = arr_alloc(&m->instances);
	*in = glm::mat4();
	*in = glm::translate(*in, pos);
	return { mid->rgroup_id, mid->mgroup_id, arr_getid(in) };
}

static bool
rid_equals(RenderID id1, RenderID id2) {
	return (id1.rgroup_id == id2.rgroup_id && id1.mgroup_id == id2.mgroup_id && id1.instance_id == id2.instance_id);
}

void
render_update_instance(RenderID rid, const glm::vec3 &pos)
{
/*
	if (rid_equals(rid, RENDER_ID_ERR))
		return;
	Model *mg = &g_rgroups[rid.rgroup_id].models[rid.mgroup_id];
	glm::mat4 *in = arr_get(mg->instances, rid.instance_id);
	*in = glm::translate(*in, pos);
*/
}

void
render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	for (int i = 0; i < num_shaders; ++i) {
		glUseProgram(g_rgroups[i].shader);
		GLint model_loc = glGetUniformLocation(g_rgroups[i].shader, "u_model");
		for (int mi = 0; mi < g_rgroups[i].num_models; ++mi) {
			Model *m = &g_rgroups[i].models[mi];
//			glActiveTexture(GL_TEXTURE0);
			glBindVertexArray(m->vao);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);
			for (glm::mat4 *in = arr_first(m->instances); in; in = arr_next(m->instances, in)) {
				glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(*in));
				GLuint num_indices_rendered = 0;
				for (int s = 0; s < m->num_meshes; ++s) {
					glBindTexture(GL_TEXTURE_2D, m->meshes[s].mat.tex.diffuse_id);
					glDrawElementsBaseVertex(GL_TRIANGLES, m->meshes[s].num_indices, GL_UNSIGNED_INT, 0, num_indices_rendered);
					num_indices_rendered += m->meshes[s].num_indices;
				}
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
/*	glBindVertexArray(0);
	glUseProgram(0);
//	glDeleteTextures(1, &g_tex);
	for (int i = 0; i < num_shaders; ++i) {
		glDeleteProgram(g_rgroups[i].shader);
		for (Model_Group *m = arr_first(g_rgroups[i].models); m; m = arr_next(g_rgroups[i].models, m)) {
			glDeleteVertexArrays(1, &m->vao);
			arr_destroy(m->instances);
		}
		arr_destroy(g_rgroups[i].models);
	}*/
}

