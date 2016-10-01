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

static GLuint g_vao, g_ebo;
static GLuint g_tex_lighting_prog, g_notex_lighting_prog, g_no_lighting_prog;
static GLuint g_tex, g_spec_tex;
static GLsizei num_indices;

static void init_glew();
static void init_gl(const int screen_width, const int screen_height);
static void init_shaders();
static void init_buffers();
static void load_textures();

constexpr int max_pt_lights = 4;
bool point_lights[max_pt_lights];

glm::vec3 pointLightPositions[] = {
	glm::vec3( 0.7f,  0.2f,  2.0f),
	glm::vec3( 2.3f, -3.3f, -4.0f),
	glm::vec3(-4.0f,  2.0f, -12.0f),
	glm::vec3( 0.0f,  0.0f, -3.0f)
};  

struct Material {

};

struct Model_Load {
	GLfloat *vertices;
	GLfloat *tex_coords;
	GLfloat *normals;
	GLuint *indices;

	GLsizei num_indices;
	GLsizei num_vertices;
	GLsizei num_tex_coords;
	GLsizei num_normals;

	bool success;
};

void
make_point_light(vec3f pos)
{
	glUseProgram(g_lighting_program);
	for (int i = 0; i < max_pt_lights; ++i) {
		if (!point_lights[i]) {
			point_lights[i] = true;
			char buf[256];
			sprintf(buf, "point_lights[%d].is_valid", i);
			printf("%s\n", buf);
			glUniform1i(glGetUniformLocation(g_lighting_program, buf), true);
			sprintf(buf, "point_lights[%d].position", i);
			glUniform3f(glGetUniformLocation(g_lighting_program, buf), pos.x, pos.y + 0.5f, pos.z);
			sprintf(buf, "point_lights[%d].ambient", i);
			glUniform3f(glGetUniformLocation(g_lighting_program, buf), 0.05f, 0.05f, 0.05f);
			sprintf(buf, "point_lights[%d].diffuse", i);
			glUniform3f(glGetUniformLocation(g_lighting_program, buf), 0.8f, 0.8f, 0.8f);
			sprintf(buf, "point_lights[%d].specular", i);
			glUniform3f(glGetUniformLocation(g_lighting_program, buf), 1.0f, 1.0f, 1.0f);
			sprintf(buf, "point_lights[%d].constant", i);
			glUniform1f(glGetUniformLocation(g_lighting_program, buf), 1.0f);
			sprintf(buf, "point_lights[%d].linear", i);
			glUniform1f(glGetUniformLocation(g_lighting_program, buf), 0.09);
			sprintf(buf, "point_lights[%d].quadratic", i);
			glUniform1f(glGetUniformLocation(g_lighting_program, buf), 0.032);
			glUseProgram(0);
			return;
		}
	}
	z_printerr("Max point lights reached!");
	glUseProgram(0);
}

//TODO: make this scan through and get counts first. make the right sized arrays and then fill them. get rid of vectors
bool
load_model(const char *fname, std::vector<GLfloat> *verts, std::vector<GLfloat> *normals, std::vector<GLuint> *indices)
{
	const char *data = z_load_file(fname);
	bool err = false;
	if (!data)
		return err;

	for (uint64_t i = 0; data[i]; ++i) {
		if (data[i] == 'v') {
			GLfloat v1, v2, v3;
			if (sscanf(&data[i+2], "%f %f %f", &v1, &v2, &v3) < 3) { // +2 to skip over the possible 'n', 't', etc.
				z_printerr("obj parser error\n");
				delete [] data;
				return err;
			}
			if (data[i+1] == 'n')
				normals->insert(normals->end(), { v1, v2, v3 });
			else
				verts->insert(verts->end(), { v1, v2, v3 });
		} else if (data[i] == 'f') {
			GLuint i1, i2, i3;
			if (sscanf(&data[i+1], "%u %u %u", &i1, &i2, &i3) < 3) {
				z_printerr("obj parser error\n");
				delete [] data;
				return err;
			}
			indices->insert(indices->end(), { i1-1, i2-1, i3-1 }); // obj indexes from 1, while opengl wants 0 indexed
		}
		while (data[i] && data[i] != '\n') ++i; // skip to the next line
	}
	delete [] data;
	return true;
}

void
init_uniforms(const int screenw, const int screenh)
{
	glUseProgram(g_lighting_program);
	// material
	glUniform1i(glGetUniformLocation(g_lighting_program, "material.diffuse"), 0);
	glUniform1i(glGetUniformLocation(g_lighting_program, "material.specular"), 1);
	glUniform1f(glGetUniformLocation(g_lighting_program, "material.shininess"), 64.0f);

	// dir lights
	glUniform3f(glGetUniformLocation(g_lighting_program, "dir_light.direction"), -0.2f, -1.0f, -0.3f);
	glUniform3f(glGetUniformLocation(g_lighting_program, "dir_light.ambient"), 0.05f, 0.05f, 0.05f);
	glUniform3f(glGetUniformLocation(g_lighting_program, "dir_light.diffuse"), 0.4f, 0.4f, 0.4f);
	glUniform3f(glGetUniformLocation(g_lighting_program, "dir_light.specular"), 0.5f, 0.5f, 0.5f);

	for (int i = 0; i < max_pt_lights; ++i) {
		char buf[256];
		sprintf(buf, "point_lights[%d].is_valid", i);
		glUniform1i(glGetUniformLocation(g_lighting_program, buf), false);
		point_lights[i] = false;
	}

	// spot light
	glUniform3f(glGetUniformLocation(g_lighting_program, "spot_light.ambient"), 0.05f, 0.05f, 0.05f);
	glUniform3f(glGetUniformLocation(g_lighting_program, "spot_light.diffuse"), 0.8f, 0.8f, 0.8f);
	glUniform3f(glGetUniformLocation(g_lighting_program, "spot_light.specular"), 1.0f, 1.0f, 1.0f);
	glUniform1f(glGetUniformLocation(g_lighting_program, "spot_light.constant"), 1.0f);
	glUniform1f(glGetUniformLocation(g_lighting_program, "spot_light.linear"), 0.09);
	glUniform1f(glGetUniformLocation(g_lighting_program, "spot_light.quadratic"), 0.032);
	glUniform1f(glGetUniformLocation(g_lighting_program, "spot_light.inner_cutoff"), glm::cos(glm::radians(12.5f)));
	glUniform1f(glGetUniformLocation(g_lighting_program, "spot_light.outer_cutoff"), glm::cos(glm::radians(17.5f)));

	glUseProgram(0);
}

void
render_init(const int screenw, const int screenh)
{
	init_gl(screenw, screenh);
	load_textures();
	init_uniforms(screenw, screenh);
}

glm::vec3 cubePositions[] = {
	glm::vec3( 0.0f,  0.5f,  0.0f),
	glm::vec3( 2.0f,  5.0f, -15.0f),
	glm::vec3(-1.5f, -2.2f, -2.5f),
	glm::vec3(-3.8f, -2.0f, -12.3f),
	glm::vec3( 2.4f, -0.4f, -3.5f),
	glm::vec3(-1.7f,  3.0f, -7.5f),
	glm::vec3( 1.3f, -2.0f, -2.5f),
	glm::vec3( 1.5f,  2.0f, -2.5f),
	glm::vec3( 1.5f,  0.2f, -1.5f),
	glm::vec3(-1.3f,  1.0f, -1.5f)
};

void
render(Camera cam)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(g_lamp_program);
	glm::mat4 view = glm::lookAt(cam.pos, cam.pos + cam.front, cam.up);
	glm::mat4 projection = glm::perspective(45.0f, (GLfloat)800 / (GLfloat)600, 0.1f, 100.0f);
	glUniformMatrix4fv(glGetUniformLocation(g_lamp_program, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(g_lamp_program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	GLint model_loc = glGetUniformLocation(g_lamp_program, "model");
	glm::mat4 model;
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(0.1f));
	glBindVertexArray(g_container_vao);
	glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
	glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
/*	glUseProgram(g_lighting_program);

	glUniform3f(glGetUniformLocation(g_lighting_program, "u_view_pos"), cam.pos.x, cam.pos.y, cam.pos.z);
        // Create camera transformations
	glm::mat4 view = glm::lookAt(cam.pos, cam.pos + cam.front, cam.up);
        glm::mat4 projection = glm::perspective(45.0f, (GLfloat)800 / (GLfloat)600, 0.1f, 100.0f);
        // Pass the matrices to the shader
        glUniformMatrix4fv(glGetUniformLocation(g_lighting_program,  "u_view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(g_lighting_program,  "u_projection"), 1, GL_FALSE, glm::value_ptr(projection));

	glUniform3f(glGetUniformLocation(g_lighting_program, "spot_light.position"), cam.pos.x, cam.pos.y, cam.pos.z);
	glUniform3f(glGetUniformLocation(g_lighting_program, "spot_light.direction"), cam.front.x, cam.front.y, cam.front.z);

        // Draw the container (using container's vertex attributes)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, g_spec_tex);
	glBindVertexArray(g_container_vao);
        GLint model_loc = glGetUniformLocation(g_lighting_program, "u_model");
	glm::mat4 model;
	for(GLuint i = 0; i < 10; i++)
	{
		model = glm::mat4();
		model = glm::translate(model, cubePositions[i]);
		GLfloat angle = 20.0f * i;
		model = glm::rotate(model, angle, glm::vec3(1.0f, 0.3f, 0.5f));
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	glUseProgram(g_lamp_program);
        // Also draw the lamp object, again binding the appropriate shader
        // Get location objects for the matrices on the lamp shader (these could be different on a different shader)
        glUniformMatrix4fv(glGetUniformLocation(g_lamp_program, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(g_lamp_program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glBindVertexArray(g_lamp_vao);
        model_loc = glGetUniformLocation(g_lamp_program, "model");
	for (GLuint i = 0; i < 4; i++)
	{
		model = glm::mat4();
		model = glm::translate(model, pointLightPositions[i]);
		model = glm::scale(model, glm::vec3(0.2f)); // Make it a smaller cube
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	glBindVertexArray(g_plane_vao);
        model_loc = glGetUniformLocation(g_lamp_program, "model");
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(20.0f));
	glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
	glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
*/
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
	ivfn (obj, objparam, &status);\
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
make_shader(const GLenum shader_type, const char *shader_str)
{
	const GLuint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, &shader_str, NULL);
	glCompileShader(shader);

	const char *shader_type_str;
	if (shader_type == GL_VERTEX_SHADER)
		shader_type_str = "vertex";
	else if (shader_type == GL_GEOMETRY_SHADER)
		shader_type_str = "geometry";
	else if (shader_type == GL_FRAGMENT_SHADER)
		shader_type_str = "fragment";
	else
		shader_type_str = "DEFAULT";
	GL_CHECK_ERR(shader, glGetShaderiv, GL_COMPILE_STATUS, glGetShaderInfoLog, "compile failure in %s shader", shader_type_str);
	return shader;
}

static GLuint
make_program(const char *vert, const char *frag)
{
	GLuint vert_shader = make_shader(GL_VERTEX_SHADER, vert);
	GLuint frag_shader = make_shader(GL_FRAGMENT_SHADER, frag);
	GLuint program = glCreateProgram();

	glAttachShader(program, vert_shader);
	glAttachShader(program, frag_shader);
	glLinkProgram(program);

	GL_CHECK_ERR(program, glGetProgramiv, GL_LINK_STATUS, glGetProgramInfoLog, "linker failure");

	glDetachShader(program, vert_shader);
	glDetachShader(program, frag_shader);
	glDeleteShader(vert_shader);
	glDeleteShader(frag_shader);

	return program;
}

static void
init_shaders()
{
	const char *light_vert = z_load_file("lighting.vert");
	assert(light_vert);
	const char *light_frag = z_load_file("lighting.frag");
	assert(light_frag);
	const char *lamp_vert = z_load_file("lamp.vert");
	assert(lamp_vert);
	const char *lamp_frag = z_load_file("lamp.frag");
	assert(light_frag);

	g_lighting_program = make_program(light_vert, light_frag);
	g_lamp_program = make_program(lamp_vert, lamp_frag);

	delete [] light_vert;
	delete [] light_frag;
	delete [] lamp_vert;
	delete [] lamp_frag;
}
/*
	GLfloat cube_verts[] = {
	    // Positions           // Normals           // Texture Coords
	    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
	     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
	     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
	     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
	    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
	    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,

	    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,
	     0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f,
	     0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
	     0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
	    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 1.0f,
	    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,

	    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
	    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
	    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
	    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
	    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
	    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

	     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
	     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
	     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
	     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
	     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
	     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

	    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
	     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
	     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
	     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
	    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
	    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

	    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
	     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
	     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
	     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
	    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
	    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f
	};

	GLfloat plane_verts[] = {
	    -1.0f,  0.0f,  -1.0f,
	     1.0f, 0.0f,  -1.0f,
	     1.0f,  0.0f,  1.0f,
	    1.0f,  0.0f,  1.0f,
	    -1.0f, 0.0f,  1.0f,
	     -1.0f, 0.0f,  -1.0f,

	};
	*/

static void
init_buffers()
{
/*******/
	bool succ;
	std::vector<GLfloat> verts;
	std::vector<GLfloat> normals;
	std::vector<GLuint> indices;
	succ = load_model("teapot.obj", &verts, &normals, &indices); //TODO: get rid of the output arguments

	num_indices = indices.size();
	// First, set the container's VAO (and VBO)
	//GLuint vaos[3];
	glGenVertexArrays(1, &g_container_vao);
	//g_lamp_vao = vaos[0];
	//g_container_vao = vaos[1];
	//g_plane_vao = vaos[2];

	GLuint vbos[2];
	//glGenBuffers(z_arr_len(vbos), vbos);
 	glGenBuffers(1, &g_cube_vbo);
	//g_cube_vbo = vbos[0];
	//g_plane_vbo = vbos[1];
	glGenBuffers(1, &g_ebo);

	//glBindBuffer(GL_ARRAY_BUFFER, g_cube_vbo);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(cube_verts), cube_verts, GL_STATIC_DRAW);
	glBindVertexArray(g_container_vao);
	glBindBuffer(GL_ARRAY_BUFFER, g_cube_vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(GLfloat), verts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

/***********
	glBindVertexArray(g_container_vao);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);

	glBindVertexArray(g_lamp_vao);
	// We only need to bind to the VBO (to link it with glVertexAttribPointer), no need to fill it; the VBO's data already contains all we need.
	glBindBuffer(GL_ARRAY_BUFFER, g_cube_vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, g_plane_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(plane_verts), plane_verts, GL_STATIC_DRAW);

	glBindVertexArray(g_plane_vao);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
**********/
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
	SDL_Surface *surface = load_surface("container2.png");

	glGenTextures(1, &g_tex);
	glBindTexture(GL_TEXTURE_2D, g_tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	SDL_FreeSurface(surface);

	surface = load_surface("container2_specular.png");

	glGenTextures(1, &g_spec_tex);
	glBindTexture(GL_TEXTURE_2D, g_spec_tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	SDL_FreeSurface(surface);

	glBindTexture(GL_TEXTURE_2D, 0);
}
