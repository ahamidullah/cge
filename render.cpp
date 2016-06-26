#include <stdio.h>
#include <GL/glew.h>
#include <assert.h>
#include <vector>
#include "zlib.h"
#include <SDL2/SDL.h>

namespace {
	const GLfloat g_verts[] = {
		0.0f,    0.5f, 0.0f, 1.0f,
		0.5f, -0.366f, 0.0f, 1.0f,
		-0.5f, -0.366f, 0.0f, 1.0f,
		1.0f,    0.0f, 0.0f, 1.0f,
		0.0f,    1.0f, 0.0f, 1.0f,
		0.0f,    0.0f, 1.0f, 1.0f,
	};
	GLfloat g_texcoords[] = {
		0.0f, 0.0f,  // Lower-left corner
		1.0f, 0.0f,  // Lower-right corner
		0.5f, 1.0f   // Top-center corner
	};
	GLuint g_vbo;
	GLuint g_vao;
	GLuint g_program;
	GLuint g_elapsed_time_uniform;
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

GLuint
create_shader(const GLenum shader_type, const char *shader_str)
{
	GLuint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, &shader_str, NULL);
	glCompileShader(shader);

	const char *shader_type_str = NULL;
	switch(shader_type)
	{
		case GL_VERTEX_SHADER: shader_type_str = "vertex"; break;
		case GL_GEOMETRY_SHADER: shader_type_str = "geometry"; break;
		case GL_FRAGMENT_SHADER: shader_type_str = "fragment"; break;
	}
	GL_CHECK_ERR(shader, glGetShaderiv, GL_COMPILE_STATUS, glGetShaderInfoLog, "compile failure in %s shader", shader_type_str);
	return shader;
}

GLuint
create_program(const std::vector<char *>& shader_strs)
{
	std::vector<GLuint> shaders(shader_strs.size());
//	GLuint shaders[num_shaders];
	GLuint program = glCreateProgram();

	shaders[0] = create_shader(GL_VERTEX_SHADER, shader_strs[0]);
	shaders[1] = create_shader(GL_FRAGMENT_SHADER, shader_strs[1]);

//	for (u32 i = 0; i < num_shaders; ++i)
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

void
init_glew()
{
	glewExperimental = true;
	GLenum err = glewInit();
	if (GLEW_OK != err)
		zlib::ABORT("%s\n", glewGetErrorString(err));
	fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
}

/*fill g_program with the shader program*/
void
init_shaders()
{
	std::vector<const char *> fnames = {
		"shader.vert",
		"shader.frag",
	};
	std::vector<char *>shader_strs;
	shader_strs.reserve(fnames.size());
	for (auto& fname : fnames) {
		FILE *fp = fopen(fname, "r");
		if (!fp)
			zlib::ABORT("error loading shader file %s!\n", fname);
		if (fseek(fp, 0L, SEEK_END) != 0)
			zlib::ABORT("could not fseek shader file %s!\n", fname);
		long filelen = ftell(fp);
		if (filelen == -1)
			zlib::ABORT("could not ftell shader file %s!\n", fname);
		char *buf = (char *)zlib::zmalloc((filelen + 1));
		if (fseek(fp, 0L, SEEK_SET) != 0) //SEEK_SET sets fp to beginning
			zlib::ABORT("could not fseek shader file %s!\n", fname);
		size_t fileend = fread(buf, sizeof(char), filelen, fp);
		if (ferror(fp))
			zlib::ABORT("error read reading file %s!", fname);
		buf[fileend+1] = '\0';
		shader_strs.push_back(buf);
		//shader_strs[i] = buf;
		fclose(fp);
	}
	g_program = create_program(shader_strs);
/*
	const char *fnames[] = {
		"shader.vert",
		"shader.frag",
	};
	char *shader_strs[ARR_LEN(fnames)];

	for (u32 i = 0; i < ARR_LEN(fnames); ++i) {
		FILE *fp = fopen(fnames[i], "r");
		if (!fp)
			zlib::ABORT("error loading shader file %s!\n", fnames[i]);
		if (fseek(fp, 0L, SEEK_END) != 0)
			zlib::ABORT("could not fseek shader file %s!\n", fnames[i]);
		long filelen = ftell(fp);
		if (filelen == -1)
			zlib::ABORT("could not ftell shader file %s!\n", fnames[i]);
		char *buf = (char *)zlib::zmalloc((filelen + 1));
		if (fseek(fp, 0L, SEEK_SET) != 0) //SEEK_SET sets fp to beginning
			zlib::ABORT("could not fseek shader file %s!\n", fnames[i]);
		size_t fileend = fread(buf, sizeof(char), filelen, fp);
		if (ferror(fp))
			zlib::ABORT("error read reading file %s!", fnames[i]);
		buf[fileend+1] = '\0';
		shader_strs[i] = buf;
		fclose(fp);
	}
	g_program = create_program(ARR_LEN(shader_strs), shader_strs);
	*/
	for (auto& str : shader_strs)
		zlib::zfree(str);
}

void
init_buffers()
{
	glGenBuffers(1, &g_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_verts), g_verts, GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glGenVertexArrays(1, &g_vao);
	glBindVertexArray(g_vao);
}

void
init_uniforms()
{
	g_elapsed_time_uniform = glGetUniformLocation(g_program, "time");

	GLuint vld = glGetUniformLocation(g_program, "loop_duration");
	GLuint fld = glGetUniformLocation(g_program, "frag_loop_duration");
	glUseProgram(g_program);
	glUniform1f(vld, 5.0f);
	glUniform1f(fld, 10.0f);
	glUseProgram(0);
}

namespace render {

void
init_gl()
{
	init_glew();
	init_shaders();
	init_buffers();
	init_uniforms();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

void
render()
{
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(g_program);
	glUniform1f(g_elapsed_time_uniform, SDL_GetTicks() / 1000.0f);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*)(48));

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	glUseProgram(0);
}

} //namespace render
