#ifndef __RENDER_H__
#define __RENDER_H__

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include <string>
#include "zlib.h"
#include "ui.h"

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

void render_init(const Camera &, const Vec2i &);
void render_update_view(const Camera &);
void render_update_projection(const Camera &, const Vec2i &);
std::optional<Render_Id> render_add_instance(std::string, const glm::vec3 &);
void render_update_instance(const Render_Id, const glm::vec3 &);
void render_sim();
void render_ui(const UI_State &);
void render_quit();
void mk_point_light(glm::vec3 pos);
std::optional<glm::vec3> raycast_plane(const glm::vec2 &screen_ray, const glm::vec3 &plane_normal, const glm::vec3 &origin, const float origin_ofs, const Vec2i &);

#endif
