#include <stdio.h>
#include <GL/glew.h>
#include <assert.h>
#include <vector>
#include "zlib.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

GLfloat g_verts[] = {
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
};
static GLuint g_indices[] = {
	0, 1, 3,
	1, 2, 3
};
static GLuint g_vao, g_vbo, g_ebo;
static GLuint g_program;
static GLuint g_tex;

internal void init_glew();
internal void init_gl(const int screen_width, const int screen_height);
internal void init_shaders();
internal void init_buffers();
internal GLuint make_program(const std::vector<std::string>& shader_strs);
internal GLuint make_shader(const GLenum shader_type, std::string shader_str);
internal void load_textures();

namespace render {

void
init(const int screenw, const int screenh)
{
	init_gl(screenw, screenh);
	load_textures();

	glUseProgram(g_program);

	// set frag shader's sampler2d to the desired texture unit
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_tex);
	glUniform1i(glGetUniformLocation(g_program, "our_tex"), 0);

	glm::mat4 model;
	model = glm::rotate(model, glm::radians(-55.0f), glm::vec3(1.0f, 0.0f, 0.0f)); 
	glm::mat4 view;
	view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f)); 
	glm::mat4 projection;
	projection = glm::perspective(glm::radians(45.0f), (float)(screenw / screenh), 0.1f, 100.0f);

	GLint modelLoc = glGetUniformLocation(g_program, "model");
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	GLuint viewLoc = glGetUniformLocation(g_program, "view");
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
	GLuint projectionLoc = glGetUniformLocation(g_program, "projection");
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

	glUseProgram(0);
}

void
render(int screenw, int screenh)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(g_program);

	glm::mat4 model;
	model = glm::rotate(model, glm::radians((GLfloat)SDL_GetTicks()/10.0f), glm::vec3(1.0f, 0.3f, 0.5f));

	GLint modelLoc = glGetUniformLocation(g_program, "model");
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));


/*	// set frag shader's sampler2d to the desired texture unit
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_tex);
	glUniform1i(glGetUniformLocation(g_program, "our_tex"), 0);

	glm::mat4 model;
	model = glm::rotate(model, glm::radians((GLfloat)SDL_GetTicks()), glm::vec3(0.5f, 1.0f, 0.0f));
	glm::mat4 view;
	view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f)); 
	glm::mat4 projection;
	projection = glm::perspective(glm::radians(45.0f), (float)(screenw / screenh), 0.1f, 100.0f);

	GLint modelLoc = glGetUniformLocation(g_program, "model");
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	GLuint viewLoc = glGetUniformLocation(g_program, "view");
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
	GLuint projectionLoc = glGetUniformLocation(g_program, "projection");
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
*/
	glBindVertexArray(g_vao);
//	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

	glUseProgram(0);
}

}

internal void
init_gl(const int screen_width, const int screen_height)
{
	init_glew();
	init_shaders();
	init_buffers();
	
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glViewport(0, 0, screen_width, screen_height);
}

internal void
init_glew()
{
	glewExperimental = true;
	GLenum err = glewInit();
	if (GLEW_OK != err)
		zlib::ABORT("%s\n", glewGetErrorString(err));
	fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
}

/*fill g_program with the shader program*/
internal void
init_shaders()
{
	const std::vector<std::string> shader_strs { zlib::load_file("shader.vert"), zlib::load_file("shader.frag") }; // ORDER MATTERS FOR NOW
	g_program = make_program(shader_strs);
}

internal void
init_buffers()
{
	glGenVertexArrays(1, &g_vao);
	glGenBuffers(1, &g_vbo);
	glGenBuffers(1, &g_ebo);
	
	glBindVertexArray(g_vao);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_verts), g_verts, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(g_indices), g_indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    // TexCoord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}

#define GL_CHECK_ERR(obj, ivfn, objparam, infofn, fmt, ...)\
{\
	GLint status;\
	ivfn (obj, objparam, &status);\
	if (status == GL_FALSE) {\
		GLint infolog_len;\
		ivfn(obj, GL_INFO_LOG_LENGTH, &infolog_len);\
		GLchar *infolog = (GLchar *)zlib::zmalloc(sizeof(GLchar)*(infolog_len + 1));\
		infofn(obj, infolog_len, NULL, infolog);\
		fprintf(stderr, "Error: ");\
		fprintf(stderr, fmt, ## __VA_ARGS__);\
		fprintf(stderr, " - %s\n", infolog);\
		zlib::zfree(infolog);\
		exit(1);\
	}\
}

internal GLuint
make_program(const std::vector<std::string>& shader_strs)
{
	const std::vector<GLuint> shaders { make_shader(GL_VERTEX_SHADER, shader_strs[0]),
                                            make_shader(GL_FRAGMENT_SHADER, shader_strs[1]) };
	GLuint program = glCreateProgram();
	for (auto sh : shaders) {
		glAttachShader(program, sh);
	}

	glLinkProgram(program);
	GL_CHECK_ERR(program, glGetProgramiv, GL_LINK_STATUS, glGetProgramInfoLog, "linker failure");

	for (auto sh : shaders) {
		glDetachShader(program, sh);
		glDeleteShader(sh);
	}
	return program;
}

internal GLuint
make_shader(const GLenum shader_type, const std::string shader_str)
{
	const GLuint shader = glCreateShader(shader_type);
	const char * shader_cstr = shader_str.c_str();
	glShaderSource(shader, 1, &shader_cstr, NULL);
	glCompileShader(shader);

	const char *shader_type_str = [shader_type] {
		switch(shader_type)
		{
			case GL_VERTEX_SHADER: return "vertex";
			case GL_GEOMETRY_SHADER: return "geometry";
			case GL_FRAGMENT_SHADER: return "fragment";
			default: return "DEFAULT";
		}
	} ();
	GL_CHECK_ERR(shader, glGetShaderiv, GL_COMPILE_STATUS, glGetShaderInfoLog, "compile failure in %s shader", shader_type_str);
	return shader;
}

internal SDL_Surface *
load_surface(const char *fname)
{
	auto flip_surface_x = [] (SDL_Surface* s)
	{
		auto getpixel = [] (SDL_Surface *surface, int x, int y)
		{
			u32 *pixels = (u32 *)surface->pixels;
			return pixels[(y * surface->w) + x];
		};
		auto putpixel = [] (SDL_Surface *surface, int x, int y, Uint32 pixel)
		{
			u32 *pixels = (u32 *)surface->pixels;
			pixels[(y * surface->w) + x] = pixel;
		};

		SDL_Surface* flip = SDL_CreateRGBSurface(0, (s)->w, s->h, s->format->BitsPerPixel, s->format->Rmask, s->format->Gmask, s->format->Bmask, s->format->Amask);	
		for(int y=0; y<s->h; y++) {
			for(int x=0; x<s->w; x++) {
				//copy pixels, but reverse the x pixels
				putpixel(flip, x, y, getpixel(s, x, s->h - y - 1));
			}
		}
		return flip;
	};

	SDL_Surface *surface;
	if (!(surface = IMG_Load(fname))) {
		zlib::PRINTERR("IMG_Load failed! IMG_GetError: %s\n", IMG_GetError);
	}

	//opengl wants the origin on the bottom, so we flip it on the x axis
	SDL_Surface *flip = flip_surface_x(surface);
	SDL_FreeSurface(surface);
	return flip;
}

internal void
load_textures()
{
	SDL_Surface *surface = load_surface("sample2.png");

	glGenTextures(1, &g_tex);
	glBindTexture(GL_TEXTURE_2D, g_tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	SDL_FreeSurface(surface);
	glBindTexture(GL_TEXTURE_2D, 0);
}
