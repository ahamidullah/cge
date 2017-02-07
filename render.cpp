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
#include <unordered_map>
#include <dirent.h>
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

struct Untextured_Mesh {
	Untextured_Mesh(GLuint ni, GLuint bv) : num_indices(ni), base_vertex(bv) {}
	GLuint num_indices;
	GLuint base_vertex;
};

struct Textured_Mesh {
	Textured_Mesh(GLuint ni, GLuint bv, GLuint did) : num_indices(ni), base_vertex(bv), diffuse_id(did) {}
	GLuint num_indices;
	GLuint base_vertex;
	GLuint diffuse_id;
};

struct Model {
	Model(GLuint pvao, GLuint pebo) : vao(pvao), ebo(pebo) {}
	GLuint                       vao;
	GLuint                       ebo;
	std::vector<Textured_Mesh>   tex_meshes;
	std::vector<Untextured_Mesh> notex_meshes;
	std::vector<glm::mat4>       instances;
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

struct Vertex {
	GLfloat position[3];
	GLfloat normal[3];
	GLfloat uv[2];
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
	Raw_Mesh_Info(GLuint ni, GLuint bv, std::optional<std::string> dp) : num_indices(ni), base_vertex(bv), diffuse_path(dp) {}
	GLuint num_indices;
	GLuint base_vertex;
	std::optional<std::string> diffuse_path;
};

struct Raw_Model {
	std::vector<Raw_Mesh_Info> raw_mesh_infos;
	std::vector<Vertex> vertices;
	std::vector<GLuint> indices;
};

struct Shader_Ids {
	GLuint tex;
	GLuint notex;
	GLuint nolight;
};

static std::unordered_map<std::string, ModelID> g_model_table;
static std::unordered_map<std::string, GLuint> g_tex_table;
static std::vector<Model> g_models;
static Shader_Ids g_shaders;
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
	for (unsigned m = 0; m < node->mNumMeshes; ++m) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[m]];
		// Assimp keeps track of indices per mesh, but we want them per model.
		GLuint index_base = rm->vertices.size();
		for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
			Vertex v;
			v.position[0] = mesh->mVertices[i].x;
			v.position[1] = mesh->mVertices[i].y;
			v.position[2] = mesh->mVertices[i].z;
			if (mesh->mNormals) {
				v.normal[0] = mesh->mNormals[i].x;
				v.normal[1] = mesh->mNormals[i].y;
				v.normal[2] = mesh->mNormals[i].z;
			}
			if (mesh->mTextureCoords[0]) {
				v.uv[0] = mesh->mTextureCoords[0][i].x;
				v.uv[1] = mesh->mTextureCoords[0][i].y;
			}
			rm->vertices.push_back(v);
		}
		int base_vertex = rm->indices.size();
		int num_mesh_inds = 0;
		for (unsigned i = 0; i < mesh->mNumFaces; ++i) {
			aiFace face = mesh->mFaces[i];
			num_mesh_inds += face.mNumIndices;
			assert(face.mNumIndices == 3);
			for (GLuint j = 0; j < face.mNumIndices; ++j) {
				rm->indices.push_back(index_base + face.mIndices[j]);
			}
		}
		// doesn't handle multiple textures per mesh for now
		aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
		aiString str;
		aiReturn has_tex = mat->GetTexture(aiTextureType_DIFFUSE, 0, &str);
		if (has_tex == AI_SUCCESS)
			rm->raw_mesh_infos.emplace_back(num_mesh_inds, base_vertex, str.C_Str());
		else
			rm->raw_mesh_infos.emplace_back(num_mesh_inds, base_vertex, std::experimental::nullopt);
	}
	for (unsigned i = 0; i < node->mNumChildren; ++i)
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

std::optional<GLuint>
get_texture(std::string path)
{
	auto it = g_tex_table.find(path);
	if (it != g_tex_table.end())
		return it->second;

	// For now, let's just gen a texture id whenever we need it. Should probably do this in batches.
	GLuint tex_id;
	glGenTextures(1, &tex_id);
	glActiveTexture(GL_TEXTURE0);
	SDL_Surface *tex;
	if (!(tex = IMG_Load(path.c_str()))) {
		zerror("IMG_Load failed! IMG_GetError: %s\n", IMG_GetError());
		return {};
	}
	printf("Loaded texture file %s\n", path.c_str());
	GLint format = tex->format->BytesPerPixel == 4 ? GL_RGBA : GL_RGB;
	glBindTexture(GL_TEXTURE_2D, tex_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, format, tex->w, tex->h, 0, format, GL_UNSIGNED_BYTE, tex->pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	SDL_FreeSurface(tex);
	glBindTexture(GL_TEXTURE_2D, 0);

	g_tex_table[path] = tex_id;
	return tex_id;
}

// loading is all temp so don't really care if it's slow
void
load_models()
{
	std::vector<std::string> paths = {
		"models/nanosuit.obj",
	};
	int num_files = paths.size();
	std::vector<GLuint> vao_ids;
	vao_ids.resize(num_files);
	std::vector<GLuint> vbo_ids;
	vbo_ids.resize(num_files);
	std::vector<GLuint> ebo_ids;
	ebo_ids.resize(num_files);
	glGenVertexArrays(num_files, vao_ids.data());
	glGenBuffers(num_files, vbo_ids.data());
	glGenBuffers(num_files, ebo_ids.data());

	Raw_Model rm;
	for (int i = 0; i < num_files; ++i) {
		Assimp::Importer import;
		const aiScene* scene = import.ReadFile(paths[i], aiProcess_Triangulate | aiProcess_FlipUVs);
		if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			zerror("Assimp error: %s", import.GetErrorString());
			continue;
		}
		process_node(scene->mRootNode, scene, &rm);

		glBindVertexArray(vao_ids[i]);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[i]);
		glBufferData(GL_ARRAY_BUFFER, rm.vertices.size()*sizeof(Vertex), rm.vertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)(sizeof(GLfloat)*3));
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(sizeof(GLfloat)*6));
		glEnableVertexAttribArray(2);

		printf("verts %d\n", rm.vertices.size());
		printf("inds %d\n", rm.indices.size());
		printf("inds %d\n", sizeof(Vertex));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_ids[i]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, rm.indices.size()*sizeof(GLuint), rm.indices.data(), GL_STATIC_DRAW);

		size_t start = paths[i].find_last_of('/')+1, end = paths[i].find_last_of('.');
		std::string base_name = paths[i].substr(start, end - start);
		g_models.emplace_back(vao_ids[i], ebo_ids[i]);
		g_model_table[base_name] = {tex_sh, g_models.size()-1};
		printf("loaded model %s\n", base_name.c_str());

		for (const auto &rmi : rm.raw_mesh_infos) {
			if (!rmi.diffuse_path) {
				g_models.back().notex_meshes.emplace_back(rmi.num_indices, rmi.base_vertex);
				continue;
			}
			std::optional<GLuint> tex_id = get_texture(*rmi.diffuse_path);
			printf("%s %d %d\n", rmi.diffuse_path->c_str(), rmi.num_indices, rmi.base_vertex);
			if (tex_id)
				g_models.back().tex_meshes.emplace_back(rmi.num_indices, rmi.base_vertex, *tex_id);
			else {
				zerror("could not find texture %s from model file %s", rmi.diffuse_path->c_str(), paths[i].c_str());
				g_models.back().notex_meshes.emplace_back(rmi.num_indices, rmi.base_vertex);
			}
		}
		rm.vertices.clear();
		rm.indices.clear();
		rm.raw_mesh_infos.clear();
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

		g_shaders.tex = mk_program(vert, frag, "#define TEX\n");
		g_shaders.notex = mk_program(vert, frag, "#define NO_TEX\n");
		g_shaders.nolight = mk_program(vert, frag, "#define NO_LIGHT\n");
		free(vert);
		free(frag);
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
		g_lights.dir.ambient = glm::vec4(1.15f, 1.15f, 1.15f, 0.0f);
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
		glUseProgram(g_shaders.notex);
		glUniform3f(glGetUniformLocation(g_shaders.notex, "mat.ambient"),  1.0f, 0.5f, 0.31f);
		glUniform3f(glGetUniformLocation(g_shaders.notex, "mat.diffuse"),  1.0f, 0.5f, 0.31f);
		glUniform3f(glGetUniformLocation(g_shaders.notex, "mat.specular"), 0.5f, 0.5f, 0.5f);
		glUniform1f(glGetUniformLocation(g_shaders.notex, "mat.shininess"), 64.0f);

		glUseProgram(g_shaders.tex);
		glUniform1i(glGetUniformLocation(g_shaders.tex, "mat.diffuse"), 0);
		//glUniform1i(glGetUniformLocation(g_rgroups[tex_sh].shader, "mat.specular"), 1);

		glUseProgram(0);
	}

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
render_add_instance(std::string name, const glm::vec3 &pos)
{
	auto it = g_model_table.find(name);
	if (it == g_model_table.end()) {
		zerror("tried to add instance of invalid model '%s'", name.c_str());
		return RENDER_ID_ERR;
	}
	ModelID mid = it->second;
	Model *m = &g_models[mid.mgroup_id];
	m->instances.emplace_back(glm::translate(glm::mat4(), pos));
	printf("added instance of %s\n", name.c_str());
	return { mid.rgroup_id, mid.mgroup_id, m->instances.size()-1 };
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

// We handle untextured meshes which slows us down (extra gl calls, extra loops).
// Should make that a debug switch in the future and have a release build that just dies if there is no texture.
void
render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GLint tex_model_loc = glGetUniformLocation(g_shaders.tex, "u_model");
	GLint notex_model_loc = glGetUniformLocation(g_shaders.notex, "u_model");
	for (const auto &model : g_models) {
		//glActiveTexture(GL_TEXTURE0);
		glBindVertexArray(model.vao);
		// TODO: replace the EBO with the VBO I think?
		//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ebo);
		for (const auto &instance : model.instances) {
			glUseProgram(g_shaders.tex);
			glUniformMatrix4fv(tex_model_loc, 1, GL_FALSE, glm::value_ptr(instance));
			for (const auto &mesh : model.tex_meshes) {
				glBindTexture(GL_TEXTURE_2D, mesh.diffuse_id);
				glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, (GLvoid *)(mesh.base_vertex * sizeof(GLuint)));
			}
			glUseProgram(g_shaders.notex);
			glUniformMatrix4fv(notex_model_loc, 1, GL_FALSE, glm::value_ptr(instance));
			for (const auto &mesh : model.notex_meshes)
				glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, (GLvoid *)(mesh.base_vertex*4));
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

