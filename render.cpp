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
#include <string>
#include <dirent.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "render.h"

constexpr int MAX_PT_LIGHTS = 4;
constexpr int MAX_MESHES = 1000;
constexpr int MAX_MODELS = 1000;

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
	Mesh_Info meshes[MAX_MESHES]; // temp
	Pool<glm::mat4> instances;
};

struct Render_Group {
	GLuint shader;
	int num_models;
	Model models[MAX_MODELS]; // temp
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
	std::string diffuse_path;
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
			// doesn't handle multiple textures per mesh for now
			aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
			aiString str;
			mat->GetTexture(aiTextureType_DIFFUSE, 0, &str);
			rm->raw_mesh_infos.push_back({num_mesh_inds, str.C_Str()});
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

// loading is all temp so don't really care if it's slow
void
load_textures()
{
	hsh_init(&g_tex_table, temp_str_hash);
	const char *dir_name = "textures/";
	int num_files = 0;
	DIR *dir;
	struct dirent *ent;
	dir = opendir(dir_name);
	if (!dir) {
		zerror("could not open directory textures/");
		return;
	}
	while ((ent = readdir(dir)) != NULL) {
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") || ent->d_type != DT_REG)
			continue;
		++num_files;
	}
	rewinddir(dir);

	GLuint *tex_ids = (GLuint *)malloc(sizeof(GLuint) * num_files);
	glGenTextures(num_files, tex_ids);
	glActiveTexture(GL_TEXTURE0);
	char path[MAX_PATH_LEN];
	int cur_file = 0;
	SDL_Surface *tex;
	while ((ent = readdir(dir)) != NULL) {
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") || ent->d_type != DT_REG)
			continue;
		strcpy(path, dir_name);
		strcat(path, ent->d_name);
		if (!(tex = IMG_Load(path))) {
			zabort("IMG_Load failed! IMG_GetError: %s\n", IMG_GetError);
			continue;
		}
		printf("Loaded texture file %s\n", path);
		GLint format = tex->format->BytesPerPixel == 4 ? GL_RGBA : GL_RGB;
		glBindTexture(GL_TEXTURE_2D, tex_ids[cur_file]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, format, tex->w, tex->h, 0, format, GL_UNSIGNED_BYTE, tex->pixels);
		glGenerateMipmap(GL_TEXTURE_2D);
		hsh_add(&g_tex_table, path, tex_ids[cur_file]);
		SDL_FreeSurface(tex);
		path[strlen(dir_name)] = '\0';
		++cur_file;
	}
	closedir(dir);
	free(tex_ids);
	glBindTexture(GL_TEXTURE_2D, 0);
}

// loading is all temp so don't really care if it's slow
void
load_models()
{
	hsh_init(&g_model_table, temp_str_hash);

	char path[MAX_PATH_LEN];
	const char *dir_name = "models/";
	strcpy(path, dir_name);
	DIR *dir;
	struct dirent *ent;
	dir = opendir(dir_name);
	int num_files = 0;
	if (!dir) {
		zerror("could not open directory models/");
		return;
	}
	while ((ent = readdir(dir)) != NULL) {
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") || ent->d_type != DT_REG)
			continue;
		if (!strcmp(ent->d_name, "nanosuit.mtl"))
			continue;
		++num_files;
	}
	rewinddir(dir);

	std::vector<GLuint> vao_ids;
	vao_ids.resize(num_files);
	std::vector<GLuint> buf_ids;
	buf_ids.resize(num_files*num_glbufs);
	glGenVertexArrays(num_files, vao_ids.data());
	glGenBuffers(num_files*num_glbufs, buf_ids.data());

	int cur_file = 0;
	Raw_Model rm;

	while ((ent = readdir(dir)) != NULL) {
		Assimp::Importer import;
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") || ent->d_type != DT_REG)
			continue;
		if (!strcmp(ent->d_name, "nanosuit.mtl")) // temp
			continue;
		strcat(path, ent->d_name);
		const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
		path[strlen(dir_name)] = '\0';
		if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			zerror("Assimp error: %s", import.GetErrorString());
			continue;
		}
		process_node(scene->mRootNode, scene, &rm);
		GLuint cur_vao = vao_ids[cur_file];
		GLuint *cur_bufs = &buf_ids[cur_file*num_glbufs];

		glBindVertexArray(cur_vao);
		glBindBuffer(GL_ARRAY_BUFFER, cur_bufs[vert_buf]);
		glBufferData(GL_ARRAY_BUFFER, rm.positions.size()*sizeof(GLfloat), rm.positions.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, cur_bufs[norm_buf]);
		glBufferData(GL_ARRAY_BUFFER, rm.normals.size()*sizeof(GLfloat), rm.normals.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cur_bufs[ind_buf]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, rm.indices.size()*sizeof(GLuint), rm.indices.data(), GL_STATIC_DRAW);

//			if (rm->tex_fname) {
			glBindBuffer(GL_ARRAY_BUFFER, cur_bufs[tex_buf]);
			glBufferData(GL_ARRAY_BUFFER, rm.uvs.size()*sizeof(GLfloat), rm.uvs.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
			glEnableVertexAttribArray(2);
//			}
		int model_ind = g_rgroups[tex_sh].num_models++;
		Model *m = &g_rgroups[tex_sh].models[model_ind];
		m->vao = cur_vao;
		m->ebo = cur_bufs[ind_buf];
		m->num_meshes = rm.raw_mesh_infos.size();
		ModelID mid =  {tex_sh, model_ind};
		char base_name[MAX_PATH_LEN];
		for (int j = 0; ent->d_name[j] != '\0'; ++j)
			base_name[j] = ent->d_name[j] == '.' ? '\0' : ent->d_name[j];
		hsh_add(&g_model_table, base_name, mid);
		pool_init(&m->instances);

		for (int i = 0; i < rm.raw_mesh_infos.size(); ++i) {
			std::optional<GLuint> tex_id = hsh_get(g_tex_table, rm.raw_mesh_infos[i].diffuse_path.c_str());
			if (tex_id)
				m->meshes[i].mat.tex.diffuse_id = *tex_id;
			else
				zerror("could not find texture %s from model file %s in tex table", rm.raw_mesh_infos[i].diffuse_path, path);
			m->meshes[i].num_indices = rm.raw_mesh_infos[i].num_indices;
		}
		rm.positions.clear();
		rm.uvs.clear();
		rm.normals.clear();
		rm.indices.clear();
		rm.raw_mesh_infos.clear();
		++cur_file;
	}
	glBindVertexArray(0);
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

	load_textures();

	load_models();
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
	glm::mat4 *in = pool_alloc(&m->instances);
	*in = glm::mat4();
	*in = glm::translate(*in, pos);
	return { mid->rgroup_id, mid->mgroup_id, pool_getid(in) };
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
	glm::mat4 *in = pool_get(mg->instances, rid.instance_id);
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
			for (glm::mat4 *in = pool_first(m->instances); in; in = pool_next(m->instances, in)) {
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
		for (Model_Group *m = pool_first(g_rgroups[i].models); m; m = pool_next(g_rgroups[i].models, m)) {
			glDeleteVertexArrays(1, &m->vao);
			pool_destroy(m->instances);
		}
		pool_destroy(g_rgroups[i].models);
	}*/
}

