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
	g_cubes[0].model.pos.x = 0.0f;
	g_cubes[0].model.pos.y = 0.0f;
	g_cubes[0].model.pos.z = 0.0f;
	g_cubes[0].model.rid = render_add_instance("nanosuit", g_cubes[0].model.pos);
}

void
update()
{
	for (int i = 0; i < NUM_CUBES; ++i) {
		//g_cubes[i].model.pos.y += .05f;
		//render_update_instance(g_cubes[i].model.rid, g_cubes[i].model.pos);
	}
}
