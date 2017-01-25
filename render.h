#ifndef __RENDER_H__
#define __RENDER_H__

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include "zlib.h"

struct RenderID {
	int rgroup_id;
	int mgroup_id;
	int instance_id;
};

struct ModelID {
	int rgroup_id;
	int mgroup_id;
};

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

void render_init(const Camera &, const Screen &);
void render_update_view(const Camera &);
RenderID render_add(const char *);
//void render_update_ent(RenderID, const glm::vec3 &);
void render(const Camera &cam);
void mk_point_light(glm::vec3 pos);
std::optional<glm::vec3> raycast_plane(const glm::vec2 &screen_ray, const glm::vec3 &plane_normal, const glm::vec3 &origin, const float origin_ofs, const Screen &);

#endif
