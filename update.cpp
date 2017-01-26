#include <glm/glm.hpp>
#include "update.h"
#include "render.h"

struct Model {
	glm::vec3 pos;
	RenderID rid;
};

struct Cube {
	Model model;
};

#define NUM_CUBES 3
Cube g_cubes[NUM_CUBES];

void
update_init()
{
	// init entities
	{
		for (int i = 0; i < NUM_CUBES; ++i) {
			g_cubes[i].model.pos.x = 10.0f * i;
			g_cubes[i].model.pos.y = 10.0f * i;
			g_cubes[i].model.pos.z = 10.0f * i;
			g_cubes[i].model.rid = render_add("cube", g_cubes[i].model.pos);
		}
	}

}

void
update()
{
	for (int i = 0; i < NUM_CUBES; ++i) {
		g_cubes[i].model.pos.y += .05f;
		render_update_instance(g_cubes[i].model.rid, g_cubes[i].model.pos);
	}
}
