#ifndef __RENDER_H__
#define __RENDER_H__

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <GL/glew.h>
#include "zlib.h"

struct Camera {
	GLfloat pitch;
	GLfloat yaw;
	GLfloat speed;
	glm::vec3 pos;
	glm::vec3 front;
	glm::vec3 up;
};

void render_init(const int screen_widht, const int screen_height);
void render(Camera);
void make_point_light(vec3f pos);

#endif
