#include <stdio.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <assert.h>
#include "sys.h"
#include "helper.h"

namespace render {

namespace {
	const GLfloat g_verts[] = {
		0.0f,    0.5f, 0.0f, 1.0f,
		0.5f, -0.366f, 0.0f, 1.0f,
		-0.5f, -0.366f, 0.0f, 1.0f,
		1.0f,    0.0f, 0.0f, 1.0f,
		0.0f,    1.0f, 0.0f, 1.0f,
		0.0f,    0.0f, 1.0f, 1.0f,
	};
	GLuint g_vbo;
	GLuint g_vao;
	GLuint g_program;
}

#define GL_CHECK_ERR(obj, ivfn, objparam, infofn, fmt, ...)\
{\
	GLint status;\
	ivfn (obj, objparam, &status);\
	if (status == GL_FALSE) {\
		GLint infolog_len;\
		ivfn(obj, GL_INFO_LOG_LENGTH, &infolog_len);\
		GLchar *infolog = (GLchar *)sys::zmalloc(sizeof(GLchar)*(infolog_len + 1));\
		infofn(obj, infolog_len, NULL, infolog);\
		fprintf(stderr, "Error: ");\
		fprintf(stderr, fmt, ## __VA_ARGS__);\
		fprintf(stderr, " - %s\n", infolog);\
		sys::zfree(infolog);\
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
create_program(const u32 num_shaders, char **shader_strs)
{
	GLuint shaders[num_shaders];
	GLuint program = glCreateProgram();

	shaders[0] = create_shader(GL_VERTEX_SHADER, shader_strs[0]);
	shaders[1] = create_shader(GL_FRAGMENT_SHADER, shader_strs[1]);

	for (u32 i = 0; i < num_shaders; i++)
		glAttachShader(program, shaders[i]);
	glLinkProgram(program);

	GL_CHECK_ERR(program, glGetProgramiv, GL_LINK_STATUS, glGetProgramInfoLog, "linker failure");

	for (u32 i = 0; i < num_shaders; i++) {
		glDetachShader(program, shaders[i]);
		glDeleteShader(shaders[i]);
	}
	return program;
}

void
init_glew()
{
	glewExperimental = true;
	GLenum err = glewInit();
	if (GLEW_OK != err)
		ABORT("%s\n", glewGetErrorString(err));
	fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
}

/*fill g_program with the shader program*/
void
init_shaders()
{
	const char *fnames[] = {
		"shader.vert",
		"shader.frag",
	};
	char *shader_strs[ARR_LEN(fnames)];

	for (u32 i = 0; i < ARR_LEN(fnames); i++) {
		FILE *fp = fopen(fnames[i], "r");
		if (!fp)
			ABORT("error loading shader file %s!\n", fnames[i]);
		if (fseek(fp, 0L, SEEK_END) != 0)
			ABORT("could not fseek shader file %s!\n", fnames[i]);
		long filelen = ftell(fp);
		if (filelen == -1)
			ABORT("could not ftell shader file %s!\n", fnames[i]);
		char *buf = (char *)sys::zmalloc((filelen + 1));
		if (fseek(fp, 0L, SEEK_SET) != 0) //SEEK_SET sets fp to beginning
			ABORT("could not fseek shader file %s!\n", fnames[i]);
		size_t fileend = fread(buf, sizeof(char), filelen, fp);
		if (ferror(fp))
			ABORT("error read reading file %s!", fnames[i]);
		buf[fileend+1] = '\0';
		shader_strs[i] = buf;
		fclose(fp);
	}
	g_program = create_program(ARR_LEN(shader_strs), shader_strs);
	for (auto str : shader_strs)
		free(str);
}

void
init_buffers()
{
	glGenBuffers(1, &g_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_verts), g_verts, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glGenVertexArrays(1, &g_vao);
	glBindVertexArray(g_vao);
}

void
init_gl()
{

	init_glew();
	init_shaders();
	init_buffers();

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

void
display(void)
{
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(g_program);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*)(48));

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	glUseProgram(0);
	glutSwapBuffers();
	glutPostRedisplay();
}

} //namespace render