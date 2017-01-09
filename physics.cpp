#include "physics.h"

std::optional<glm::vec3>
raycast_plane(const glm::vec2 &screen_ray, const glm::vec3 &plane_normal, const glm::vec3 &origin, const float origin_ofs, const Screen &scr)
{
	extern glm::mat4 g_projection_mat;
	extern glm::mat4 g_view_mat;

	float x = (2.0f * screen_ray.x) / scr.w - 1.0f;
	float y = 1.0f - (2.0f * screen_ray.y) / scr.h;
	glm::vec4 clip_ray = glm::vec4(x, y, -1.0f, 1.0f);
//	glm::mat4 projection = glm::perspective(fov, screen_width / screen_height, near_plane, far_plane);
	glm::vec4 eye_ray = glm::inverse(g_projection_mat) * clip_ray;
	eye_ray = glm::vec4(eye_ray.x, eye_ray.y, -1.0f, 0.0f);
//	glm::mat4 view = glm::lookAt(cam.pos, cam.pos + cam.front, cam.up);
	glm::vec3 world_ray = glm::normalize((glm::inverse(g_view_mat) * eye_ray).xyz());
	float l = glm::dot(world_ray, plane_normal);
	if (l >= 0.0f && l <= 0.001f) // perpendicular
		return {};
	float t = -((glm::dot(origin, glm::vec3(0.0f, 1.0f, 0.0f)) + origin_ofs) / l);
	if (t < 0.0f) // intersected behind
		return {};
	glm::vec3 pos = origin + world_ray * t;
	return pos;
}
