constexpr int MAX_PT_LIGHTS = 4;
constexpr int MAX_UI_VERTICES = 100;
constexpr int MAX_GLYPH_VERTICES = 1000;

struct Render_Id {
	size_t model_ind;
	size_t instance_ind;
};

struct Camera {
	float pitch;
	float yaw;
	float speed;
	Vec3f pos;
	Vec3f front;
	Vec3f up;

	float fov;
	float near;
	float far;
};

struct Untextured_Mesh {
	GLuint num_indices;
	GLuint base_vertex;
};

struct Textured_Mesh {
	GLuint num_indices;
	GLuint base_vertex;
	GLuint diffuse_id;
	GLuint specular_id;
};

struct Model_Vertex {
	GLfloat position[3];
	GLfloat normal[3];
	GLfloat uv[2];
};

struct Dir_Light {
/*
	glm::vec4 direction;
	glm::vec4 ambient;
	glm::vec4 diffuse;
	glm::vec4 specular;
*/
	Vec4f direction;
	Vec4f ambient;
	Vec4f diffuse;
	Vec4f specular;
};

struct Spot_Light {
	// TODO: pack the floats into the last component of the glm::vec4
	//glm::vec4 position;
	//glm::vec4 direction;
	//glm::vec4 ambient;
	//glm::vec4 diffuse;
	//glm::vec4 specular;

	Vec4f position;
	Vec4f direction;
	Vec4f ambient;
	Vec4f diffuse;
	Vec4f specular;

	float outer_cutoff;
	float inner_cutoff;
	float constant;
	float linear;
	float quadratic;
};

struct Point_Light {    
/*
	glm::vec4 position;
	glm::vec4 ambient;
	glm::vec4 diffuse;
	glm::vec4 specular;
	*/
	Vec4f position;
	Vec4f ambient;
	Vec4f diffuse;
	Vec4f specular;
	float constant;
	float linear;
	float quadratic;
	bool is_valid;
};

// TODO: possible for CPU and GPU UBO structs to get out of sync. Worth it to move them into a single header and set up some macros?

// CPU copy of matrix struct ubo on GPU.
// Order and alignment have to match!
struct Matrices {
	Mat4 view;
	Mat4 perspective_proj;
	Mat4 ortho_proj;
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

struct Glyph_Info {
	Glyph_Vertex canonical_vertices[6];
	float advance;
};

/*
struct Font_Info {
	GLuint vao;
	GLuint vbo;
	GLuint texture;
	//std::vector<Glyph_Vertex> vertices;
	Array<Glyph_Vertex> vertices;
	std::unordered_map<char, Glyph_Info> glyph_map;
};
*/

struct Ubo_Ids {
	GLuint matrices;
	GLuint lights;
};

struct Shader_Ids {
	GLuint textured_mesh;
	GLuint untextured_mesh;
	GLuint ui;
	GLuint text;
};

struct Model_Asset {
	GLuint vao;
	GLuint vbo;
	GLuint num_meshes;
	Textured_Mesh meshes[0]; // Struct hack -- is "meshes[1]" better?
};

struct Model_Instance {
	//glm::mat4 pos;
	Mat4 pos;
};

struct Loaded_Assets {
	void **lookup_table;
	GLuint *mesh_textures;
	Memory_Arena models;
};

Memory_Arena g_static_render_memory = mem_make_arena();

Loaded_Assets
init_assets()
{
	Asset_File_Header af_header;
	File_Handle af_handle = platform_open_file("assets.ahh", "");
	DEFER(platform_close_file(af_handle));
	platform_read(af_handle, sizeof(Asset_File_Header), &af_header);

	Loaded_Assets la;
	//la.num_assets = NUM_NAMED_ASSET_IDS + af_header.num_mesh_textures;
	la.models = mem_make_arena();
	// TODO: Need to zero out the mesh texture memory.
	la.lookup_table = mem_alloc_array(void *, NUM_NAMED_ASSET_IDS + af_header.num_mesh_textures, &g_static_render_memory);
	la.mesh_textures = mem_alloc_array(GLuint, af_header.num_mesh_textures, &g_static_render_memory);
	return la;
}

Loaded_Assets g_assets = init_assets();

// TODO: Probably better to create one list of model instances. Each instance keeps its Model_ID and we just sort the list once when we start rendering.
Memory_Arena g_model_instances[NUM_MODEL_IDS];

static Shader_Ids g_shaders;
static Lights g_lights;
static Matrices g_matrices;
static Ubo_Ids g_ubos;
static UI_Render_Info g_ui_render_info;

size_t
get_mesh_tex_asset_id(size_t mtex_table_ind)
{
	return NUM_NAMED_ASSET_IDS + mtex_table_ind;
}

/*
V3f
screen_to_world_ray(V2u screen_pos, V2i screen_dim)
{
	float x = (2.0f * screen_pos.x) / screen_dim.x - 1.0f;
	float y = 1.0f - (2.0f * screen_pos.y) / screen_dim.y;
	Vec4f clip_ray = { x, y, -1.0f, 1.0f }; // OpenGL specific, -1 is away from the screen.
	inverse(g_matrices.view * g_matrices.perspective_proj);
}
*/

/*
V3f
raycast_plane_intersect(const V2u &screen_ray, const V3f &plane_normal, const V3f &origin, const float origin_ofs, const V2i &screen_dim)
{
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
*/

/*
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
*/

#define GL_CHECK_ERR(obj, ivfn, objparam, infofn, fmt, ...)\
{\
	GLint status;\
	ivfn(obj, objparam, &status);\
	if (status == GL_FALSE) {\
		GLint infolog_len;\
		ivfn(obj, GL_INFO_LOG_LENGTH, &infolog_len);\
		GLchar *infolog = new GLchar [infolog_len + 1];\
		infofn(obj, infolog_len, NULL, infolog);\
		debug_print("Error: ");\
		debug_print(fmt, ## __VA_ARGS__);\
		debug_print(" - %s\n", infolog);\
		delete infolog;\
		_exit(1);\
	}\
}

static GLuint
compile_shader(const GLenum type, const char *src, const char *defines)
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
make_gpu_program(const char *vert_src, const char *frag_src, const char *defines)
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

/*
static std::optional<GLuint>
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
*/

// @TEMP
#include <stdlib.h>

static GLuint
get_mesh_texture(uint32_t mtex_table_ind, GLuint gl_tex_unit, Memory_Arena *arena)
{
	if (mtex_table_ind == (uint32_t) -1)
		return 0;

	size_t asset_id = get_mesh_tex_asset_id(mtex_table_ind);
	if (g_assets.lookup_table[asset_id])
		return *(GLuint *)g_assets.lookup_table[asset_id];

	GLuint tex_id;
	glGenTextures(1, &tex_id);
	glActiveTexture(gl_tex_unit);
	Asset_File_Header af_header;
	File_Handle af_handle = platform_open_file("assets.ahh", "");
	DEFER(platform_close_file(af_handle));
	platform_read(af_handle, sizeof(Asset_File_Header), &af_header);
	uint32_t af_tex_offset;
	platform_file_seek(af_handle, af_header.mesh_texture_table_offset + sizeof(uint32_t)*mtex_table_ind);
	platform_read(af_handle, sizeof(uint32_t), &af_tex_offset);
	platform_file_seek(af_handle, af_tex_offset);
	uint8_t bytes_per_pixel;
	int w, h, pitch;
	platform_read(af_handle, sizeof(bytes_per_pixel), &bytes_per_pixel);
	platform_read(af_handle, sizeof(w), &w);
	platform_read(af_handle, sizeof(h), &h);
	platform_read(af_handle, sizeof(pitch), &pitch);
	size_t nbytes = h * pitch;
	char *pixels = mem_alloc_array(char, nbytes, arena);
	DEFER(mem_free(arena, pixels));
	platform_read(af_handle, nbytes, pixels);
	GLint format = bytes_per_pixel == 4 ? GL_RGBA : GL_RGB;
	glBindTexture(GL_TEXTURE_2D, tex_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	g_assets.mesh_textures[mtex_table_ind] = tex_id;
	g_assets.lookup_table[asset_id] = &g_assets.mesh_textures[mtex_table_ind];
	return tex_id;
}

Model_Asset *
get_model(Model_ID id)
{
	if (g_assets.lookup_table[id])
		return (Model_Asset *)g_assets.lookup_table[id];
	TIMED_BLOCK(load_model_from_disk);
	// TODO: store this header instead of reading it in everytime.
	Asset_File_Header af_header;
	File_Handle af_handle = platform_open_file("assets.ahh", "");
	DEFER(platform_close_file(af_handle));
	platform_read(af_handle, sizeof(Asset_File_Header), &af_header);

	Memory_Arena load_arena = mem_make_arena();
	DEFER(mem_destroy_arena(&load_arena));
	uint32_t af_model_ofs, num_verts, num_indices, num_meshes; 
	platform_file_seek(af_handle, af_header.named_asset_offsets[id]);
	platform_read(af_handle, sizeof(uint32_t), &num_verts);
	float *vert_buf = mem_alloc_array(float, num_verts, &load_arena);
	platform_read(af_handle, sizeof(uint32_t), &num_indices);
	unsigned *ind_buf = mem_alloc_array(unsigned, num_indices, &load_arena);
	platform_read(af_handle, sizeof(Model_Vertex)*num_verts, vert_buf);
	platform_read(af_handle, sizeof(uint32_t)*num_indices, ind_buf);
	platform_read(af_handle, sizeof(uint32_t), &num_meshes);

	Model_Asset *model = (Model_Asset *)mem_push(sizeof(Model_Asset) + (sizeof(Textured_Mesh)*num_meshes), &g_assets.models);
	model->num_meshes = num_meshes;

	for (int i = 0; i < num_meshes; ++i) {
		// TODO: Should these be 32 bit or 64 bit?
		uint32_t diff_index, spec_index;
		platform_read(af_handle, sizeof(uint32_t), &model->meshes[i].num_indices);
		platform_read(af_handle, sizeof(uint32_t), &model->meshes[i].base_vertex);
		platform_read(af_handle, sizeof(uint32_t), &spec_index);
		platform_read(af_handle, sizeof(uint32_t), &diff_index);
		model->meshes[i].specular_id = get_mesh_texture(spec_index, GL_TEXTURE1, &load_arena);
		model->meshes[i].diffuse_id = get_mesh_texture(diff_index, GL_TEXTURE0, &load_arena);
/*
		if (diffuse_asset_id == (uint64_t)-1) {
			g_models.back().notex_meshes.emplace_back(mesh_num_indices, mesh_base_vert);
			continue;
		}
		// We use texture target zero if we can't get a specular texture, which opengl says is "the default texture" in that texture unit.
		// Is that ok? (I think so).
		g_models.back().tex_meshes.emplace_back(mesh_num_indices, mesh_base_vert, get_tex_id(diffuse_asset_id, GL_TEXTURE0), get_tex_id(specular_asset_id, GL_TEXTURE1));
*/
	}

	// TODO: Worth it to load these up front?
	GLuint ebo;
	glGenVertexArrays(1, &model->vao);
	glGenBuffers(1, &model->vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(model->vao);
	glBindBuffer(GL_ARRAY_BUFFER, model->vbo);
	glBufferData(GL_ARRAY_BUFFER, num_verts*sizeof(Model_Vertex), vert_buf, GL_STATIC_DRAW);

	// TODO: Change these to offsetof()s.
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Model_Vertex), (GLvoid *)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Model_Vertex), (GLvoid *)(sizeof(GLfloat)*3));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Model_Vertex), (GLvoid*)(sizeof(GLfloat)*6));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_indices*sizeof(GLuint), ind_buf, GL_STATIC_DRAW);

	glBindVertexArray(0);
	g_assets.lookup_table[id] = model;
	return model;
}

/*
// Font texture packing.
struct Pack_Node {
	Vec2u available;
	Vec2s upper_left;
	Pack_Node *l, *r;
};

#define IS_LEAF(n) (n->l == NULL && n->r == NULL)

static void
blit_glyph(const FT_Bitmap &bm, const Vec2s upper_left, const size_t row_len, unsigned char *buf)
{
	for (size_t i = 0, buf_row = upper_left.y; i < bm.rows; ++i, ++buf_row)
		for (size_t j = 0, buf_col = upper_left.x; j < (unsigned)std::abs(bm.pitch); ++j, ++buf_col)
			buf[buf_row*row_len+buf_col] = bm.buffer[i*bm.pitch+j];
}

static std::optional<Vec2s>
find_space(Pack_Node *n, Vec2u wanted)
{
	if (IS_LEAF(n) && wanted.x <= n->available.x && wanted.y <= n->available.y) {
		Pack_Node *l_node = (Pack_Node *)malloc(sizeof(Pack_Node));
		Pack_Node *r_node = (Pack_Node *)malloc(sizeof(Pack_Node));
		// We always allocate the requested block in the upper left of the node's available space.
		// How should we split the rest of the space in the node? Simple heuristic is just split vertically if the space requested is
		// longer than it is wide, otherwise split horizontally.
		// The left node is always the split part adjacent to the allocated block (to the right when split horizontally and below
		// when split vertically). Nodes with a zero dimension are possible and are ignored.
		if (wanted.y >= wanted.x) { // vertical split
			l_node->available = {wanted.x, n->available.y - wanted.y};
			r_node->available = {n->available.x - wanted.x, n->available.y};
			l_node->upper_left = {n->upper_left.x, n->upper_left.y + wanted.y};
			r_node->upper_left = {n->upper_left.x + wanted.x, n->upper_left.y};
		} else { // horizontal split
			l_node->available = {n->available.x - wanted.x, wanted.y};
			r_node->available = {n->available.x, n->available.y - wanted.y};
			l_node->upper_left = {n->upper_left.x + wanted.x, n->upper_left.y};
			r_node->upper_left = {n->upper_left.x, n->upper_left.y + wanted.y};
		}
		l_node->l = l_node->r = r_node->l = r_node->r = NULL;
		n->l = l_node;
		n->r = r_node;
		return n->upper_left; // where we should start the blit
	}
	if (IS_LEAF(n))
		return {};
	std::optional<Vec2s> l = find_space(n->l, wanted); 
	if (l)
		return l;
	return find_space(n->r, wanted);
}

static void
free_pack_tree(Pack_Node *n)
{
	if (n->l)
		free_pack_tree(n->l);
	if (n->r)
		free_pack_tree(n->r);
	free(n);
}

static void
pack_font(const FT_Library &ft, const char *font_path, const Vec2u tex_dim, const Vec2f screen_scale, std::unordered_map<char, Glyph_Info> &glyph_map, unsigned char *buf)
{
	FT_Face face;
	if (FT_New_Face(ft, font_path, 0, &face))
		zabort("failed to load font %s", font_path);
	DEFER(FT_Done_Face(face));
	FT_Set_Pixel_Sizes(face, 0, 24);

	Pack_Node *root = (Pack_Node *)malloc(sizeof(Pack_Node));
	root->available = tex_dim;
	root->upper_left = {0, 0};
	root->l = NULL; root->r = NULL;
	DEFER(free_pack_tree(root));

	for (unsigned char c = 0; c < 128; ++c) {
		if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
			zerror("FreeType failed to load character %c", c);
			continue;
		}
		std::optional<Vec2s> tex_upper_left = find_space(root, {(unsigned)std::abs(face->glyph->bitmap.pitch), face->glyph->bitmap.rows});
		if (!tex_upper_left) {
			zerror("failed to pack font");
			return;
		}
		blit_glyph(face->glyph->bitmap, *tex_upper_left, tex_dim.x, buf);

		Glyph_Info *g = &glyph_map[c];
		float x0 = face->glyph->bitmap_left * screen_scale.x;
		float x1 = x0 + (face->glyph->bitmap.width * screen_scale.x);
		float y0 = (face->glyph->bitmap.rows - face->glyph->bitmap_top) * screen_scale.y;
		float y1 = y0 - (face->glyph->bitmap.rows * screen_scale.y);
		float u0 = (float)tex_upper_left->x / tex_dim.x;
		float u1 = ((float)tex_upper_left->x + face->glyph->bitmap.width) / tex_dim.x;
		float v0 = ((float)tex_upper_left->y + face->glyph->bitmap.rows) / tex_dim.y;
		float v1 = (float)tex_upper_left->y / tex_dim.y;
		g->canonical_vertices[0] = { {x0, y1, 0.0f}, {u0, v1}};
		g->canonical_vertices[1] = { {x0, y0, 0.0f}, {u0, v0}};
		g->canonical_vertices[2] = { {x1, y0, 0.0f}, {u1, v0}};
		g->canonical_vertices[3] = { {x0, y1, 0.0f}, {u0, v1}};
		g->canonical_vertices[4] = { {x1, y0, 0.0f}, {u1, v0}};
		g->canonical_vertices[5] = { {x1, y1, 0.0f}, {u1, v1}};
		g->advance = (face->glyph->advance.x >> 6) * screen_scale.x; // FreeType stores advance in 1/64th pixels.
	}
}
*/

void
render_update_view(const Camera &cam)
{
	g_matrices.view = view_matrix(cam.pos, cam.front);

	glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.matrices);
	glBufferSubData(GL_UNIFORM_BUFFER, offsetof(Matrices, view), sizeof(g_matrices.view), &g_matrices.view);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	// TODO: We could split this function up and avoid updating the view pos when only our facing direction changes
	glUseProgram(g_shaders.textured_mesh);
	glUniform3f(glGetUniformLocation(g_shaders.textured_mesh, "u_view_pos"), cam.pos.x, cam.pos.y, cam.pos.z);
	glUseProgram(0);
}

void
render_update_projection(const Camera &cam, const Vec2u &screen_dim)
{
	g_matrices.perspective_proj = perspective_matrix(cam.fov, (float)screen_dim.x / (float)screen_dim.y, cam.near, cam.far);
	g_matrices.ortho_proj = ortho_matrix(0.0f, 100.0f, 100.0f, 0.0f);
	//g_matrices.ortho_proj = glm::ortho(0.0f, (float)screen_dim.x, (float)screen_dim.y, 0.0f);
	glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.matrices);
	// TODO: We could combine these calls (will break if Matrices changes)
	glBufferSubData(GL_UNIFORM_BUFFER, offsetof(Matrices, perspective_proj), sizeof(g_matrices.perspective_proj), &g_matrices.perspective_proj);
	glBufferSubData(GL_UNIFORM_BUFFER, offsetof(Matrices, ortho_proj), sizeof(g_matrices.ortho_proj), &g_matrices.ortho_proj);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

// Init.
void
render_init(const Camera &cam, const Vec2u &screen_dim)
{
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glViewport(0, 0, screen_dim.x, screen_dim.y);

	for (int i = 0; i < NUM_MODEL_IDS; ++i) {
		g_model_instances[i] = mem_make_arena();
	}

	Memory_Arena init_arena = mem_make_arena();
	DEFER(mem_destroy_arena(&init_arena));

	// init shaders
	{
		const char *vert = platform_read_entire_file("assets/shaders/shader.vert", &init_arena);
		assert(vert);
		const char *frag = platform_read_entire_file("assets/shaders/shader.frag", &init_arena);
		assert(frag);

		g_shaders.textured_mesh = make_gpu_program(vert, frag, "#define TEXTURED_MESH\n");
		g_shaders.untextured_mesh = make_gpu_program(vert, frag, "#define UNTEXTURED_MESH\n");
		g_shaders.ui = make_gpu_program(vert, frag, "#define UI\n");
		g_shaders.text = make_gpu_program(vert, frag, "#define TEXT\n");
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

		glBindBuffer(GL_UNIFORM_BUFFER, g_ubos.matrices);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(Matrices), 0, GL_STATIC_DRAW);
		//glBindBuffer(GL_UNIFORM_BUFFER, 0);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_ubos.matrices);

		render_update_view(cam);
		render_update_projection(cam, screen_dim);
	}

	// init light ubo
	{
		set_bind_pt(g_shaders.textured_mesh, "Lights", 1);
		set_bind_pt(g_shaders.untextured_mesh, "Lights", 1);
		glGenBuffers(1, &g_ubos.lights);

		g_lights.dir.direction = { -2.2f, 0.0f, -1.3f, 0.0f };
		g_lights.dir.ambient = { 0.15f, 0.15f, 0.15f, 0.0f };
		g_lights.dir.diffuse = { 0.4f, 0.4f, 0.4f, 0.0f };
		g_lights.dir.specular = { 1.5f, 1.5f, 1.5f, 0.0f };
		g_lights.spot.ambient = { 0.05f, 0.05f, 0.05f, 0.0f };
		g_lights.spot.diffuse = { 0.8f, 0.8f, 0.8f, 0.0f };
		g_lights.spot.specular = { 1.0f, 1.0f, 1.0f, 0.0f };
		g_lights.spot.outer_cutoff = _cos(17.5f * M_DEG_TO_RAD);
		g_lights.spot.inner_cutoff = _cos(12.5f * M_DEG_TO_RAD);
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

/*
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
		// TODO: handle proper utf8
		FT_Library ft;
		if (FT_Init_FreeType(&ft))
			zabort("could not init freetype lib");
		DEFER(FT_Done_FreeType(ft));
		const char *font_paths[] = {
			"fonts/arial.ttf",
			"fonts/Anonymous Pro.ttf",
		};
		constexpr unsigned tex_size = 512; 
		unsigned char tex_buf[tex_size*tex_size];
		char base_name[512];
		for (const char *path : font_paths) {
			bool bn = get_base_name(path, base_name);
			assert(bn);
			Font *font = &g_font_map[base_name];
			array_init(&font->vertices, MAX_GLYPH_VERTICES);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			pack_font(ft, path, {tex_size, tex_size}, {100.0f/screen_dim.x, 100.0f/screen_dim.y}, font->glyph_map, tex_buf);

			glGenTextures(1, &font->texture);
			glBindTexture(GL_TEXTURE_2D, font->texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, tex_size, tex_size, 0, GL_RED, GL_UNSIGNED_BYTE, tex_buf);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glGenVertexArrays(1, &font->vao);
			glGenBuffers(1, &font->vbo);

			glBindVertexArray(font->vao);
			glBindBuffer(GL_ARRAY_BUFFER, font->vbo);
			glBufferData(GL_ARRAY_BUFFER, MAX_GLYPH_VERTICES*sizeof(Glyph_Vertex), NULL, GL_DYNAMIC_DRAW);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Glyph_Vertex), (GLvoid *)offsetof(Glyph_Vertex, position));
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Glyph_Vertex), (GLvoid *)offsetof(Glyph_Vertex, uv));

			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
		}

		glUseProgram(g_shaders.text);
		glUniform1i(glGetUniformLocation(g_shaders.text, "u_texture"), 0);
	}

*/
	//load_models();
}

void
render_add_instance(Model_ID id, Vec3f pos)
{
	Model_Instance *i = mem_alloc(Model_Instance, &g_model_instances[id]);
	i->pos = translate(make_mat4(), pos);
/*
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
*/
}

/*
void
render_update_instance(const Render_Id rid, const glm::vec3 &offset)
{
	assert(rid.model_ind < g_models.size());
	assert(rid.instance_ind < g_models[rid.model_ind].instances.size());
	Model *m = &g_models[rid.model_ind];
	glm::mat4 *in = &m->instances[rid.instance_ind];
	*in = glm::translate(*in, offset);
}
*/

// TODO:
// - We handle untextured meshes which slows us down (extra gl calls, extra loops).
//   Should make that a debug switch in the future and have a release build that just dies if there is no texture.
// - Is it faster to draw all similar meshes at once (less texture switching but more model uniform switching)
// - Might be better to have an "active_models" list that copies all models with instances. 
// - Should put all instances into a single list with a model_id and just sort the list before render.
void
render_sim()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// TODO: Put these locations in the Shader struct.
	GLint tex_model_loc = glGetUniformLocation(g_shaders.textured_mesh, "u_model");
	GLint notex_model_loc = glGetUniformLocation(g_shaders.untextured_mesh, "u_model");
	for (int i = 0; i < NUM_MODEL_IDS; ++i) {
		Model_Asset *model;
		GLuint num_meshes;
		if (mem_has_elems(&g_model_instances[i])) {
			model = get_model((Model_ID)i);
			glBindVertexArray(model->vao);
			glBindBuffer(GL_ARRAY_BUFFER, model->vbo);
			num_meshes = model->num_meshes;
		}
		for (Model_Instance *inst = (Model_Instance *)mem_start(&g_model_instances[i]); inst; inst = (Model_Instance *)mem_next(inst)) {
			glUseProgram(g_shaders.textured_mesh);
			glUniformMatrix4fv(tex_model_loc, 1, GL_FALSE, &inst->pos[0]);
			for (int j = 0; j < num_meshes; ++j) {
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, model->meshes[j].diffuse_id);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, model->meshes[j].specular_id);
				glDrawElements(GL_TRIANGLES, model->meshes[j].num_indices, GL_UNSIGNED_INT, (GLvoid *)(model->meshes[j].base_vertex * sizeof(GLuint)));
			}
		}
	}
}

/*
#define VERTS_PER_GLYPH 6

static void
push_glyph(Glyph_Info g, float *x, float *y, Glyph_Vertex *buf)
{
	for (int i = 0; i < VERTS_PER_GLYPH; ++i) {
		buf[i].position.x = g.canonical_vertices[i].position.x + *x;
		buf[i].position.y = g.canonical_vertices[i].position.y + *y;
		buf[i].position.z = 0.0f;
		buf[i].uv.x = g.canonical_vertices[i].uv.x;
		buf[i].uv.y = g.canonical_vertices[i].uv.y;
	}
	*x += g.advance;
}

*/
void
render_ui()
{
	glClear(GL_DEPTH_BUFFER_BIT);
	/*assert(ui.vertices.size() <= MAX_UI_VERTICES);
	glUseProgram(g_shaders.ui);
	glBindVertexArray(g_ui_render_info.vao);
	glBindBuffer(GL_ARRAY_BUFFER, g_ui_render_info.vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, ui.vertices.size()*sizeof(UI_Vertex), ui.vertices.data());
	glDrawArrays(GL_TRIANGLES, 0, ui.vertices.size());

	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	char t[] = "the quick brown fox jumped over the lazy dog";
	float x = 0.0f, y = 10.0f;
	auto itr = g_font_map.find("arial");
	assert(itr != g_font_map.end());
	Font *font = &itr->second;

	array_clear(&font->vertices);
	glUseProgram(g_shaders.text);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, font->texture);
	glBindVertexArray(font->vao);
	glBindBuffer(GL_ARRAY_BUFFER, font->vbo);
	for (char *text = t; *text; ++text) {
		auto gitr = font->glyph_map.find(*text);
		assert(gitr != font->glyph_map.end());
		push_glyph(gitr->second, &x, &y, array_alloc(&font->vertices, VERTS_PER_GLYPH));
	}
	glBufferSubData(GL_ARRAY_BUFFER, 0, font->vertices.size*sizeof(Glyph_Vertex), font->vertices.elems);
	glDrawArrays(GL_TRIANGLES, 0, font->vertices.size);
*/
}

void
render_quit()
{
/*
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
*/
}
