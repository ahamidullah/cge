#if defined(DEFINEPROC) && defined(LOADPROC)
#error "Both DEFINEPROC and LOADPROC are define. (Forgot to undef one?)"
#elif defined(DEFINEPROC)
#define GLPROC(name, ret, ...)\
	typedef ret (*name##_GLPROC)(__VA_ARGS__);\
	name##_GLPROC name = NULL
#elif defined(LOADPROC)
#define GLPROC(name, ret, ...)\
	name = (name##_GLPROC)glXGetProcAddress((const GLubyte *)#name);\
	if (!name) zabort("failed to load OpenGL function %s" #name)
#else 
#error "Missing DEFINEPROC or LOADPROC."
#endif

//GLPROC(glXCreateContextAttribsARB, GLXContext, Display*, GLXFBConfig, GLXContext, Bool, const int*)

//GLPROC(glEnable, void, GLenum);
//GLPROC(glDisable, void, GLenum);
//GLPROC(glBlendFunc, void, GLenum, GLenum);
//GLPROC(glClearColor, void, GLclampf, GLclampf, GLclampf, GLclampf);
//GLPROC(glClear, GLbitfield);
//GLPROC(glViewport, void, GLint, GLint, GLsizei, GLsizei);

GLPROC(glGetUniformBlockIndex, GLuint, GLuint, const GLchar *);
GLPROC(glUniformBlockBinding, void, GLuint, GLuint, GLuint);
GLPROC(glGetUniformLocation, GLint, GLuint, const GLchar *);
GLPROC(glUniformMatrix4fv, void, GLint, GLsizei, GLboolean, const GLfloat *);
GLPROC(glUniform3f, void, GLint, GLfloat, GLfloat, GLfloat);
GLPROC(glUniform1f, void, GLint, GLfloat);
GLPROC(glUniform1i, void, GLint, GLint);

GLPROC(glGenVertexArrays,    void,   GLsizei, GLuint *);
GLPROC(glGenBuffers,    void,   GLsizei, GLuint *);
GLPROC(glBindVertexArray,    void,   GLuint);
GLPROC(glEnableVertexAttribArray,    void,   GLuint);
GLPROC(glVertexAttribPointer,    void,   GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *);
GLPROC(glBindBuffer,    void,   GLenum, GLuint);
GLPROC(glBindBufferBase,    void,   GLenum, GLuint, GLuint);
GLPROC(glBufferSubData, void,   GLenum, GLintptr, GLsizeiptr, const GLvoid *);
GLPROC(glBufferData, void,   GLenum, GLsizeiptr, const GLvoid *, GLenum);
GLPROC(glDeleteVertexArray,    void, GLsizei,   const GLuint *);

GLPROC(glCreateShader,  GLuint, GLenum);
GLPROC(glShaderSource, void, GLuint, GLsizei, const GLchar **, const GLint *);
GLPROC(glCompileShader, void, GLuint);
GLPROC(glGetShaderiv,   void,   GLuint, GLenum, GLint *);
GLPROC(glGetShaderInfoLog, void, GLuint, GLsizei, GLsizei *, GLchar *);
GLPROC(glAttachShader,    void,   GLuint, GLuint);
GLPROC(glDetachShader,    void,   GLuint, GLuint);
GLPROC(glDeleteShader,    void,   GLuint);

GLPROC(glCreateProgram, GLuint, void);
GLPROC(glLinkProgram,    void,   GLuint);
GLPROC(glGetProgramiv,   void,   GLuint, GLenum, GLint *);
GLPROC(glGetProgramInfoLog, void, GLuint, GLsizei, GLsizei *, GLchar *);
GLPROC(glUseProgram,    void,   GLuint);
//
//GLPROC(glGetTextures, void, GLsizei, GLuint *);
//GLPROC(glTexImage2d, void, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *);
//GLPROC(glBindTexture, void, GLuint);
//GLPROC(glTexParameteri, void, GLenum, GLenum, GLint);
GLPROC(glGenerateMipmap, void, GLenum);

//GLPROC(glDrawElements, void, GLenum, GLsizei, GLenum, const GLvoid *);
//GLPROC(glDrawArrays, void, GLenum, GLint, GLsizei);

#undef GLPROC
