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
			g_cubes[i].model.pos.x = 30.0f * i;
			g_cubes[i].model.pos.y = 30.0f * i;
			g_cubes[i].model.pos.z = 30.0f * i;
			g_cubes[i].model.rid = render_add("cube");
		}
//		g_entities.count = 0;
//		g_entities.next_key = 1;
//		g_entities.max_used = 0;
//		g_entities.free_head = 0;
	}

}

void
update()
{
/*	for (int i = 0; i < NUM_CUBES; ++i) {
		g_cubes[i].pos.y += .01f;
		render_update_ent(g_cubes[i].ent.rid, g_cubes[i].pos);
	}
*/
}
