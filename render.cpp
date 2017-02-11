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
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "render.h"

constexpr int MAX_PT_LIGHTS = 4;
constexpr int MAX_UI_VERTICES = 100;

struct Untextured_Mesh {
	Untextured_Mesh(GLuint ni, GLuint bv) : num_indices(ni), base_vertex(bv) {}
	GLuint num_indices;
	GLuint base_vertex;
};

struct Textured_Mesh {
	Textured_Mesh(GLuint ni, GLuint bv, GLuint did, GLuint sid) : num_indices(ni), base_vertex(bv), diffuse_id(did), specular_id(sid) {}
	GLuint num_indices;
	GLuint base_vertex;
	GLuint diffuse_id;
	GLuint specular_id;
};

struct Model {
	Model(GLuint pvao, GLuint pvbo) : vao(pvao), vbo(pvbo) {}
	GLuint                       vao;
	GLuint                       vbo;
	std::vector<Textured_Mesh>   tex_meshes;
	std::vector<Untextured_Mesh> notex_meshes;
	std::vector<glm::mat4>       instances;
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

// TODO: possible for CPU and GPU UBO structs to get out of sync. Worth it to move them into a single header and set up some macros?

// CPU copy of matrix struct ubo on GPU.
// Order and alignment have to match!
struct Matrices {
	glm::mat4 view;
	glm::mat4 perspective_proj;
	glm::mat4 ortho_proj;
};

// Cpu copy of frag shader ubo light structures.
// Order and alignment have to match!
struct Lights {
	alignas(16) Dir_Light dir;
	alignas(16) Spot_Light spot;
	alignas(16) Point_Light points[MAX_PT_LIGHTS];
};

struct UI_Render_Info {
	GLuint vao;
	GLuint vbo;
};

struct Ubo_Ids {
	GLuint matrices;
	GLuint lights;
};

struct Raw_Mesh_Info {
	Raw_Mesh_Info(GLuint ni, GLuint bv, std::optional<std::string> dp, std::optional<std::string> sp) : num_indices(ni), base_vertex(bv), diffuse_path(dp), specular_path(sp) {}
	GLuint num_indices;
	GLuint base_vertex;
	std::optional<std::string> diffuse_path;
	std::optional<std::string> specular_path;
};

struct Raw_Model {
	std::vector<Raw_Mesh_Info> raw_mesh_infos;
	std::vector<Vertex> vertices;
	std::vector<GLuint> indices;
};

struct Shader_Ids {
	GLuint tex;
	GLuint notex;
	GLuint ui;
};

static std::unordered_map<std::string, size_t> g_model_map;
static std::unordered_map<std::string, GLuint> g_tex_map;
static std::vector<Model> g_models;
static Shader_Ids g_shaders;
static Lights g_lights;
static Matrices g_matrices;
static Ubo_Ids g_ubos;
static UI_Render_Info g_ui_render_info;

std::optional<glm::vec3>
raycast_plane(const glm::vec2 &screen_ray, const glm::vec3 &plane_normal, const glm::vec3 &origin, const float origin_ofs, const Screen &scr)
{
	float x = (2.0f * screen_ray.x) / scr.w - 1.0f;
	float y = 1.0f - (2.0f * screen_ray.y) / scr.h;
	glm::vec4 clip_ray = glm::vec4(x, y, -1.0f, 1.0f);
	glm::vec4 eye_ray = glm::inverse(g_matrices.perspective_proj) * clip_ray;
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
		GLuint base_index = rm->vertices.size();
		assert(mesh->HasPositions() && mesh->HasNormals() && mesh->HasFaces());
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
			assert(face.mNumIndices == 3);
			num_mesh_inds += face.mNumIndices;
			for (GLuint j = 0; j < face.mNumIndices; ++j) {
				rm->indices.push_back(base_index + face.mIndices[j]);
			}
		}
		// Doesn't handle multiple textures per mesh.
		aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
		aiString diffuse_path, specular_path;
		aiReturn has_diffuse = mat->GetTexture(aiTextureType_DIFFUSE, 0, &diffuse_path);
		aiReturn has_specular = mat->GetTexture(aiTextureType_DIFFUSE, 0, &specular_path);
		if (has_diffuse == AI_SUCCESS && has_specular == AI_SUCCESS)
			rm->raw_mesh_infos.emplace_back(num_mesh_inds, base_vertex, diffuse_path.C_Str(), specular_path.C_Str());
		else if (has_diffuse == AI_SUCCESS)
			rm->raw_mesh_infos.emplace_back(num_mesh_inds, base_vertex, diffuse_path.C_Str(), std::nullopt);
		else if (has_specular == AI_SUCCESS)
			rm->raw_mesh_infos.emplace_back(num_mesh_inds, base_vertex, std::nullopt, specular_path.C_Str());
		else
			rm->raw_mesh_infos.emplace_back(num_mesh_inds, base_vertex, std::nullopt, std::nullopt);
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
compile_shader(const GLenum type, const std::string &src, const char *defines)
{
	const GLuint id = glCreateShader(type);
	const char *src_strs[] = { "#version 330\n", defines, src.c_str() };
	glShaderSource(id, sizeof(src_strs)/sizeof(src_strs[0]), src_strs, NULL);
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
make_gpu_program(const std::string &vert_src, const std::string &frag_src, const char *defines)
{
	GLuint vert_id = compile_shader(GL_VERTEX_SHADER, vert_src, defines);
	GLuint frag_id = compile_shader(GL_FRAGMENT_SHADER, frag_src, defines);
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
get_texture(std::string path, GLuint gl_tex_unit)
{
	auto it = g_tex_map.find(path);
	if (it != g_tex_map.end())
		return it->second;

	// Load the texture from disk.
	// For now, let's just gen a texture id whenever we need it. Should probably do this in batches.
	GLuint tex_id;
	glGenTextures(1, &tex_id);
	glActiveTexture(gl_tex_unit);
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

	g_tex_map[path] = tex_id;
	return tex_id;
}

// Loading is all temp so don't really care if it's slow
void
load_models()
{
	std::vector<std::string> paths = {
		"models/nanosuit.obj",
	};
	int num_files = paths.size();

	std::vector<GLuint> vao_ids;
	std::vector<GLuint> vbo_ids;
	std::vector<GLuint> ebo_ids;

	vao_ids.resize(num_files);
	vbo_ids.resize(num_files);
	ebo_ids.resize(num_files);

	glGenVertexArrays(num_files, vao_ids.data());
	glGenBuffers(num_files, vbo_ids.data());
	glGenBuffers(num_files, ebo_ids.data());

	Raw_Model rm;
	for (int i = 0; i < num_files; ++i) {
		Assimp::Importer import;
		const aiScene* scene = import.ReadFile(paths[i], aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
		if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			zerror("Assimp error: %s", import.GetErrorString());
			continue;
		}
		process_node(scene->mRootNode, scene, &rm);

		glBindVertexArray(vao_ids[i]);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[i]);
		glBufferData(GL_ARRAY_BUFFER, rm.vertices.size()*sizeof(Vertex), rm.vertices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)(sizeof(GLfloat)*3));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(sizeof(GLfloat)*6));

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_ids[i]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, rm.indices.size()*sizeof(GLuint), rm.indices.data(), GL_STATIC_DRAW);

		size_t start = paths[i].find_last_of('/')+1, end = paths[i].find_last_of('.');
		std::string base_name = paths[i].substr(start, end - start);
		g_models.emplace_back(vao_ids[i], vbo_ids[i]);
		g_model_map[base_name] = g_models.size()-1;
		printf("loaded model %s\n", base_name.c_str());

		for (const auto &rmi : rm.raw_mesh_infos) {
			if (!rmi.diffuse_path) {
				g_models.back().notex_meshes.emplace_back(rmi.num_indices, rmi.base_vertex);
				continue;
			}

			// We use texture target zero if we can't get a specular texture, which opengl says is "the default texture" in that texture unit.
			// Is that ok? (I think so).
			std::optional<GLuint> spec_id = 0;
			if (rmi.specular_path) {
				spec_id = get_texture(*rmi.specular_path, GL_TEXTURE1);
				if (!spec_id) {
					zerror("could not find texture %s from model file %s", rmi.specular_path->c_str(), paths[i].c_str());
					spec_id = 0;
				}
			}

			std::optional<GLuint> diff_id = get_texture(*rmi.diffuse_path, GL_TEXTURE0);
			if (diff_id)
				g_models.back().tex_meshes.emplace_back(rmi.num_indices, rmi.base_vertex, *diff_id, *spec_id);
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
		std::optional<std::string> vert = load_file("shaders/shader.vert", "r");
		assert(vert);
		std::optional<std::string> frag = load_file("shaders/shader.frag", "r");
		assert(frag);

		g_shaders.tex = make_gpu_program(*vert, *frag, "#define TEX\n");
		g_shaders.notex = make_gpu_program(*vert, *frag, "#define NO_TEX\n");
		g_shaders.ui = make_gpu_program(*vert, *frag, "#define UI\n");
	}

	auto set_bind_pt = [](const GLuint program, const char *name, const GLuint bind_pt) {
		const GLuint ind = glGetUniformBlockIndex(program, name);
		glUniformBlockBinding(program, ind, bind_pt);
	};

	// init matrix ubo
	{
		set_bind_pt(g_shaders.tex, "Matrices", 0);
		set_bind_pt(g_shaders.notex, "Matrices", 0);
		set_bind_pt(g_shaders.ui, "Matrices", 0);
		glGenBuffers(1, &g_ubos.matrices);

		render_update_view(cam);
		render_update_projection(cam, scr);
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

		g_lights.dir.direction = glm::vec4(-2.2f, 0.0f, -1.3f, 0.0f);
		g_lights.dir.ambient = glm::vec4(0.15f, 0.15f, 0.15f, 0.0f);
		g_lights.dir.diffuse = glm::vec4(0.4f, 0.4f, 0.4f, 0.0f);
		g_lights.dir.specular = glm::vec4(1.5f, 1.5f, 1.5f, 0.0f);
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
		glUniform1i(glGetUniformLocation(g_shaders.tex, "mat.specular"), 1);
		glUniform1f(glGetUniformLocation(g_shaders.tex, "mat.shininess"), 64.0f);

		glUseProgram(0);
	}

	// init ui render info
	{
		glGenVertexArrays(1, &g_ui_render_info.vao);
		glGenBuffers(1, &g_ui_render_info.vbo);

		glBindVertexArray(g_ui_render_info.vao);
		glBindBuffer(GL_ARRAY_BUFFER, g_ui_render_info.vbo);
		glBufferData(GL_ARRAY_BUFFER, MAX_UI_VERTICES*sizeof(UI_Vertex), NULL, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(UI_Vertex), (GLvoid *)offsetof(UI_Vertex, position));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(UI_Vertex), (GLvoid *)offsetof(UI_Vertex, color));

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
	}

	load_models();
}

void
render_update_view(const Camera &cam)
{
	g_matrices.view = glm::lookAt(cam.pos, cam.pos + cam.front, cam.up);
	glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.matrices);
	glBufferSubData(GL_UNIFORM_BUFFER, offsetof(Matrices, view), sizeof(g_matrices.view), glm::value_ptr(g_matrices.view));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	// TODO: We could split this function up and avoid updating the view pos when only our facing direction changes
	glUseProgram(g_shaders.tex);
	glUniform3f(glGetUniformLocation(g_shaders.tex, "u_view_pos"), cam.pos.x, cam.pos.y, cam.pos.z);
	glUseProgram(0);

}

void
render_update_projection(const Camera &cam, const Screen &scr)
{
	g_matrices.perspective_proj = glm::perspective(cam.fov, (float)scr.w / (float)scr.h, cam.near, cam.far);
	g_matrices.ortho_proj = glm::ortho(0.0f, 100.0f, 100.0f, 0.0f);
	glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.matrices);
	// TODO: We could combine these calls (will break if Matrices changes)
	glBufferSubData(GL_UNIFORM_BUFFER, offsetof(Matrices, perspective_proj), sizeof(g_matrices.perspective_proj), glm::value_ptr(g_matrices.perspective_proj));
	glBufferSubData(GL_UNIFORM_BUFFER, offsetof(Matrices, ortho_proj), sizeof(g_matrices.ortho_proj), glm::value_ptr(g_matrices.ortho_proj));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

std::optional<Render_Id>
render_add_instance(std::string name, const glm::vec3 &pos)
{
	auto it = g_model_map.find(name);
	if (it == g_model_map.end()) {
		zerror("tried to add instance of invalid model '%s'", name.c_str());
		return {};
	}
	Model *m = &g_models[it->second];
	m->instances.emplace_back(glm::translate(glm::mat4(), pos));
	printf("added instance of %s\n", name.c_str());
	Render_Id rid = { it->second, m->instances.size()-1 };
	return rid;
}

void
render_update_instance(const Render_Id rid, const glm::vec3 &offset)
{
	assert(rid.model_ind < g_models.size());
	assert(rid.instance_ind < g_models[rid.model_ind].instances.size());
	Model *m = &g_models[rid.model_ind];
	glm::mat4 *in = &m->instances[rid.instance_ind];
	*in = glm::translate(*in, offset);
}

// We handle untextured meshes which slows us down (extra gl calls, extra loops).
// Should make that a debug switch in the future and have a release build that just dies if there is no texture.
// Is it faster to draw all similar meshes at once (less texture switching but more model uniform switching)
void
render_sim()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GLint tex_model_loc = glGetUniformLocation(g_shaders.tex, "u_model");
	GLint notex_model_loc = glGetUniformLocation(g_shaders.notex, "u_model");
	for (const auto &model : g_models) {
		glBindVertexArray(model.vao);
		glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
		for (const auto &instance : model.instances) {
			glUseProgram(g_shaders.tex);
			glUniformMatrix4fv(tex_model_loc, 1, GL_FALSE, glm::value_ptr(instance));
			for (const auto &mesh : model.tex_meshes) {
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, mesh.diffuse_id);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, mesh.specular_id);
				glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, (GLvoid *)(mesh.base_vertex * sizeof(GLuint)));
			}
			glUseProgram(g_shaders.notex);
			glUniformMatrix4fv(notex_model_loc, 1, GL_FALSE, glm::value_ptr(instance));
			for (const auto &mesh : model.notex_meshes)
				glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, (GLvoid *)(mesh.base_vertex * sizeof(GLuint)));
		}
	}
}

void
render_ui(const UI_State &ui)
{
	glClear(GL_DEPTH_BUFFER_BIT);
	assert(ui.vertices.size() <= MAX_UI_VERTICES);
	glUseProgram(g_shaders.ui);
	glBindVertexArray(g_ui_render_info.vao);
	glBindBuffer(GL_ARRAY_BUFFER, g_ui_render_info.vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, ui.vertices.size()*sizeof(UI_Vertex), ui.vertices.data());
	glDrawArrays(GL_TRIANGLES, 0, ui.vertices.size());
}

void
render_quit()
{
	glBindVertexArray(0);
	glUseProgram(0);
	glDeleteProgram(g_shaders.tex);
	glDeleteProgram(g_shaders.notex);
	glDeleteProgram(g_shaders.ui);
	for (const auto &model : g_models) {
		glDeleteVertexArrays(1, &model.vao);
		glDeleteBuffers(1, &model.vbo);
		for (const auto &mesh : model.tex_meshes) {
			glDeleteTextures(1, &mesh.diffuse_id);
			glDeleteTextures(1, &mesh.specular_id);
		}
	}
}
