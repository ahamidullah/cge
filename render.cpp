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

#define STB_RECT_PACK_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#include "stb_truetype.h"
#pragma GCC diagnostic pop

#include "render.h"

constexpr int MAX_PT_LIGHTS = 4;
constexpr int MAX_UI_VERTICES = 100;
constexpr int MAX_TEXT_VERTICES = 1000;

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

struct Model_Vertex {
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

struct Glyph_Vertex {
	Vec3f position;
	Vec2f uv;
};

struct Glyph {
	Glyph_Vertex vertices[6];
	float x_advance;
};

struct Font {
	GLuint vao;
	GLuint vbo;
	GLuint texture;
	std::vector<Glyph_Vertex> glyph_buffer;
	//std::vector<stbtt_packedchar> char_data;
	std::unordered_map<char, Glyph> glyph_map;
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
	std::vector<Model_Vertex> vertices;
	std::vector<GLuint> indices;
};

struct Shader_Ids {
	GLuint textured_mesh;
	GLuint untextured_mesh;
	GLuint ui;
	GLuint text;
};

static std::unordered_map<std::string, size_t> g_model_map;
static std::unordered_map<std::string, GLuint> g_tex_map;
static std::unordered_map<std::string, Font> g_font_map;
static std::vector<Model> g_models;
static Shader_Ids g_shaders;
static Lights g_lights;
static Matrices g_matrices;
static Ubo_Ids g_ubos;
static UI_Render_Info g_ui_render_info;

std::optional<glm::vec3>
raycast_plane(const glm::vec2 &screen_ray, const glm::vec3 &plane_normal, const glm::vec3 &origin, const float origin_ofs, const Vec2i &screen_dim)
{
	float x = (2.0f * screen_ray.x) / screen_dim.x - 1.0f;
	float y = 1.0f - (2.0f * screen_ray.y) / screen_dim.y;
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
			Model_Vertex v;
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

#define GET_BASE_NAME(path) path.substr(path.find_last_of('/')+1, path.find_last_of('.') - (path.find_last_of('/')+1))

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
		glBufferData(GL_ARRAY_BUFFER, rm.vertices.size()*sizeof(Model_Vertex), rm.vertices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Model_Vertex), (GLvoid *)0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Model_Vertex), (GLvoid *)(sizeof(GLfloat)*3));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Model_Vertex), (GLvoid*)(sizeof(GLfloat)*6));

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_ids[i]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, rm.indices.size()*sizeof(GLuint), rm.indices.data(), GL_STATIC_DRAW);

		//size_t start = paths[i].find_last_of('/')+1, end = paths[i].find_last_of('.');
		g_models.emplace_back(vao_ids[i], vbo_ids[i]);
		g_model_map[GET_BASE_NAME(paths[i])] = g_models.size()-1;
		printf("loaded model %s\n", GET_BASE_NAME(paths[i]).c_str());

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
render_init(const Camera &cam, const Vec2i &screen_dim)
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
	glViewport(0, 0, screen_dim.x, screen_dim.y);

	// init shaders
	{
		std::optional<std::string> vert = load_file("shaders/shader.vert", "r");
		assert(vert);
		std::optional<std::string> frag = load_file("shaders/shader.frag", "r");
		assert(frag);

		g_shaders.textured_mesh = make_gpu_program(*vert, *frag, "#define TEXTURED_MESH\n");
		g_shaders.untextured_mesh = make_gpu_program(*vert, *frag, "#define UNTEXTURED_MESH\n");
		g_shaders.ui = make_gpu_program(*vert, *frag, "#define UI\n");
		g_shaders.text = make_gpu_program(*vert, *frag, "#define TEXT\n");
	}

	auto set_bind_pt = [](const GLuint program, const char *name, const GLuint bind_pt) {
		const GLuint ind = glGetUniformBlockIndex(program, name);
		glUniformBlockBinding(program, ind, bind_pt);
	};

	// init matrix ubo
	{
		set_bind_pt(g_shaders.textured_mesh, "Matrices", 0);
		set_bind_pt(g_shaders.untextured_mesh, "Matrices", 0);
		set_bind_pt(g_shaders.ui, "Matrices", 0);
		glGenBuffers(1, &g_ubos.matrices);

		render_update_view(cam);
		render_update_projection(cam, screen_dim);
		glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.matrices);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(Matrices), &g_matrices, GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_ubos.matrices);
	}

	// init light ubo
	{
		set_bind_pt(g_shaders.textured_mesh, "Lights", 1);
		set_bind_pt(g_shaders.untextured_mesh, "Lights", 1);
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
		glUseProgram(g_shaders.untextured_mesh);
		glUniform3f(glGetUniformLocation(g_shaders.untextured_mesh, "mat.ambient"),  1.0f, 0.5f, 0.31f);
		glUniform3f(glGetUniformLocation(g_shaders.untextured_mesh, "mat.diffuse"),  1.0f, 0.5f, 0.31f);
		glUniform3f(glGetUniformLocation(g_shaders.untextured_mesh, "mat.specular"), 0.5f, 0.5f, 0.5f);
		glUniform1f(glGetUniformLocation(g_shaders.untextured_mesh, "mat.shininess"), 64.0f);

		glUseProgram(g_shaders.textured_mesh);
		glUniform1i(glGetUniformLocation(g_shaders.textured_mesh, "mat.diffuse"), 0);
		glUniform1i(glGetUniformLocation(g_shaders.textured_mesh, "mat.specular"), 1);
		glUniform1f(glGetUniformLocation(g_shaders.textured_mesh, "mat.shininess"), 64.0f);

		glUseProgram(0);
	}

	// init ui render info
	{
		// TODO: use index buffer for ui vertices
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

	// load fonts
	{
		std::vector<std::string> font_paths = {
			"fonts/Anonymous Pro.ttf"
		};
		// TODO: use index buffer for font vertices
		unsigned char atlas_buf[512*512]; // temp
		unsigned char ttf_buf[1<<20]; // temp
		for (auto fp : font_paths) {
			glUseProgram(g_shaders.text);
			char first_char = ' ', last_char = '~';

			// rasterize the font
			fread(ttf_buf, 1, 1<<20, fopen(fp.c_str(), "rb"));
			Font *font = &g_font_map[GET_BASE_NAME(fp)];
			//font->char_data.resize(last_char - first_char);
			stbtt_packedchar char_data[last_char - first_char];
			stbtt_pack_context context;
			if (!stbtt_PackBegin(&context, atlas_buf, 512, 512, 0, 1, NULL))
				zabort("Failed to initialize font %s", fp.c_str());
			stbtt_PackSetOversampling(&context, 2, 2);
			if (!stbtt_PackFontRange(&context, ttf_buf, 0, 32, first_char, last_char - first_char, char_data))
				zabort("Failed to pack font %s", fp.c_str());
			stbtt_PackEnd(&context);

			glGenTextures(1, &font->texture);
			glBindTexture(GL_TEXTURE_2D, font->texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, atlas_buf);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
			glGenerateMipmap(GL_TEXTURE_2D);

			glGenVertexArrays(1, &font->vao);
			glGenBuffers(1, &font->vbo);

			glBindVertexArray(font->vao);
			glBindBuffer(GL_ARRAY_BUFFER, font->vbo);
			glBufferData(GL_ARRAY_BUFFER, MAX_TEXT_VERTICES*sizeof(Glyph_Vertex), NULL, GL_STATIC_DRAW);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Glyph_Vertex), (GLvoid *)offsetof(Glyph_Vertex, position));
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Glyph_Vertex), (GLvoid *)offsetof(Glyph_Vertex, uv));

			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);

			glUniform1i(glGetUniformLocation(g_shaders.text, "u_texture"), 0);

			// stbtt gives us positions in screen space, but our ortho projection is from 0.0f to 100.0f, so we have to normalize
			float scale_x = 100.0f / screen_dim.x, scale_y = 100.0f / screen_dim.y; 
			stbtt_aligned_quad q;
			for (char ch = first_char; ch <= last_char; ++ch) {
				float x = 0.0f, y = 0.0f;
				float x_advance = stbtt_GetPackedQuad(char_data, 512, 512, ch - first_char, &x, &y, &q, 0);

				Glyph *g = &font->glyph_map[ch];
				// TODO: use an index buffer
				g->vertices[0] = { {q.x0*scale_x, q.y1*scale_y, 0.0f}, {q.s0, q.t1}};
				g->vertices[1] = { {q.x0*scale_x, q.y0*scale_y, 0.0f}, {q.s0, q.t0}};
				g->vertices[2] = { {q.x1*scale_x, q.y0*scale_y, 0.0f}, {q.s1, q.t0}};
				g->vertices[3] = { {q.x0*scale_x, q.y1*scale_y, 0.0f}, {q.s0, q.t1}};
				g->vertices[4] = { {q.x1*scale_x, q.y0*scale_y, 0.0f}, {q.s1, q.t0}};
				g->vertices[5] = { {q.x1*scale_x, q.y1*scale_y, 0.0f}, {q.s1, q.t1}};
				g->x_advance = x_advance*scale_x;
			}
		}
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
	glUseProgram(g_shaders.textured_mesh);
	glUniform3f(glGetUniformLocation(g_shaders.textured_mesh, "u_view_pos"), cam.pos.x, cam.pos.y, cam.pos.z);
	glUseProgram(0);

}

void
render_update_projection(const Camera &cam, const Vec2i &screen_dim)
{
	g_matrices.perspective_proj = glm::perspective(cam.fov, (float)screen_dim.x / (float)screen_dim.y, cam.near, cam.far);
	g_matrices.ortho_proj = glm::ortho(0.0f, 100.0f, 100.0f, 0.0f);
	//g_matrices.ortho_proj = glm::ortho(0.0f, (float)screen_dim.x, (float)screen_dim.y, 0.0f);
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
	GLint tex_model_loc = glGetUniformLocation(g_shaders.textured_mesh, "u_model");
	GLint notex_model_loc = glGetUniformLocation(g_shaders.untextured_mesh, "u_model");
	for (const auto &model : g_models) {
		glBindVertexArray(model.vao);
		glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
		for (const auto &instance : model.instances) {
			glUseProgram(g_shaders.textured_mesh);
			glUniformMatrix4fv(tex_model_loc, 1, GL_FALSE, glm::value_ptr(instance));
			for (const auto &mesh : model.tex_meshes) {
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, mesh.diffuse_id);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, mesh.specular_id);
				glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, (GLvoid *)(mesh.base_vertex * sizeof(GLuint)));
			}
			glUseProgram(g_shaders.untextured_mesh);
			glUniformMatrix4fv(notex_model_loc, 1, GL_FALSE, glm::value_ptr(instance));
			for (const auto &mesh : model.notex_meshes)
				glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, (GLvoid *)(mesh.base_vertex * sizeof(GLuint)));
		}
	}
}

#define VERTS_PER_GLYPH 6

Glyph
offset_glyph(Glyph g, float *x, float *y)
{
	for (int i = 0; i < VERTS_PER_GLYPH; ++i) {
		g.vertices[i].position.x += *x;
		g.vertices[i].position.y += *y;
	}
	*x += g.x_advance;
	return g;
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

	glDisable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	char t[] = "the quick brown fox jumped over the lazy dog";
	float x = 0.0f, y = 10.0f;
	auto itr = g_font_map.find("Anonymous Pro");
	assert(itr != g_font_map.end());
	Font *font = &itr->second;

	font->glyph_buffer.clear();
	glUseProgram(g_shaders.text);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, font->texture);
	glBindVertexArray(font->vao);
	glBindBuffer(GL_ARRAY_BUFFER, font->vbo);
	for (char *text = t; *text; ++text) {
/*
		stbtt_aligned_quad q;
		stbtt_GetPackedQuad(font->char_data.data(), 512, 512, *text - ' ', &x, &y, &q, 0); // 1=opengl&d3d10+, 0=d3d9

		// TODO: use an index buffer
		font->glyph_buffer.push_back({ {q.x0, q.y1, 0.0f}, {q.s0, q.t1}});
		font->glyph_buffer.push_back({ {q.x0, q.y0, 0.0f}, {q.s0, q.t0}});
		font->glyph_buffer.push_back({ {q.x1, q.y0, 0.0f}, {q.s1, q.t0}});

		font->glyph_buffer.push_back({ {q.x0, q.y1, 0.0f}, {q.s0, q.t1}});
		font->glyph_buffer.push_back({ {q.x1, q.y0, 0.0f}, {q.s1, q.t0}});
		font->glyph_buffer.push_back({ {q.x1, q.y1, 0.0f}, {q.s1, q.t1}});
*/
		auto gitr = font->glyph_map.find(*text);
		assert(gitr != font->glyph_map.end());
		Glyph ofs_glyph = offset_glyph(gitr->second, &x, &y);
		font->glyph_buffer.insert(font->glyph_buffer.end(), ofs_glyph.vertices, ofs_glyph.vertices + VERTS_PER_GLYPH);
	}
	glBufferSubData(GL_ARRAY_BUFFER, 0, font->glyph_buffer.size()*sizeof(Glyph_Vertex), font->glyph_buffer.data());
	glDrawArrays(GL_TRIANGLES, 0, font->glyph_buffer.size());
}

void
render_quit()
{
	glBindVertexArray(0);
	glUseProgram(0);
	glDeleteProgram(g_shaders.textured_mesh);
	glDeleteProgram(g_shaders.untextured_mesh);
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
