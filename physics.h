#ifndef __PHYSICS_H__
#define __PHYSICS_H__

#include "render.h"

std::optional<glm::vec3> raycast_plane(const glm::vec2 &screen_ray, const glm::vec3 &plane_normal, const glm::vec3 &origin, const float origin_ofs, const Screen &);

#endif
