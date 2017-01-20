#ifndef __RENDER_H__
#define __RENDER_H__

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include "zlib.h"

struct Camera {
	float pitch;
	float yaw;
	float speed;
	glm::vec3 pos;
	glm::vec3 front;
	glm::vec3 up;

	float fov;
	float near;
	float far;
};

struct Screen {
	int w;
	int h;
};

//float screen_width = 800.0f;
//float screen_height = 600.0f;
//static float fov = 45.0f;
//static float near_plane = 0.1f;
//static float far_plane = 100.0f;

void render_init(const Camera &, const Screen &);
void update_render_view(const Camera &);
void render(const Camera &cam);
void mk_point_light(glm::vec3 pos);
std::optional<glm::vec3> raycast_plane(const glm::vec2 &screen_ray, const glm::vec3 &plane_normal, const glm::vec3 &origin, const float origin_ofs, const Screen &);

#endif
