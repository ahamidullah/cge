#include <stdio.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include "sys.h"

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

#define gl_check_err(obj, ivfn, objparam, infofn, fmt, ...)\
{\
	GLint status;\
	ivfn (obj, objparam, &status);\
	if (status == GL_FALSE) {\
		GLint infolog_len;\
		ivfn(obj, GL_INFO_LOG_LENGTH, &infolog_len);\
		GLchar *infolog = (GLchar *)sys::cge_malloc(sizeof(GLchar)*(infolog_len + 1));\
		infofn(obj, infolog_len, NULL, infolog);\
		fprintf(stderr, "Error: ");\
		fprintf(stderr, fmt, ## __VA_ARGS__);\
		fprintf(stderr, " - %s\n", infolog);\
		sys::cge_free(infolog);\
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
	gl_check_err(shader, glGetShaderiv, GL_COMPILE_STATUS, glGetShaderInfoLog, "compile failure in %s shader", shader_type_str);
	return shader;
}

GLuint
create_program(const char *vert, const char *frag)
{
	int num_shaders = 2;
	GLuint shader_list[num_shaders];
	GLuint program = glCreateProgram();

	shader_list[0] = create_shader(GL_VERTEX_SHADER, vert);
	shader_list[1] = create_shader(GL_FRAGMENT_SHADER, frag);

	for (int i = 0; i < num_shaders; i++)
		glAttachShader(program, shader_list[i]);
	glLinkProgram(program);

	gl_check_err(program, glGetProgramiv, GL_LINK_STATUS, glGetProgramInfoLog, "linker failure");

	for (int i = 0; i < num_shaders; i++) {
		glDetachShader(program, shader_list[i]);
		glDeleteShader(shader_list[i]);
	}
	return program;
}

void
init_gl()
{
	glewExperimental = true;
	GLenum err = glewInit();
	if (GLEW_OK != err)
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
	fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

	const char *vert =
		"#version 330\n"
		"layout(location = 0) in vec4 pos;\n"
		"layout(location = 1) in vec4 in_color;\n"
		"\n"
		"smooth out vec4 vert_color;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = pos;\n"
		"   vert_color = in_color;\n"
		"}\n";
	const char *frag =
		"#version 330\n"
		"smooth in vec4 vert_color;\n"
		"\n"
		"out vec4 out_color;\n"
		"void main()\n"
		"{\n"
		//"   out_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);\n"
		"   out_color = vert_color;\n"
		"}\n";
	char *vert = (const char *)sys::cge_malloc(sys::file_len("shader.vert"));
	char *frag = (const char *)sys::cge_malloc(sys::file_len("shader.frag"));
	g_program = create_program(load_file(vert, "shader.vert"), load_file(frag, "shader.frag");
	free(vert);
	free(frag);

	glGenBuffers(1, &g_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_verts), g_verts, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glGenVertexArrays(1, &g_vao);
	glBindVertexArray(g_vao);
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