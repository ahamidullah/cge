#include <glm/glm.hpp>
#include "update.h"
#include "render.h"

struct Model {
	Render_Id rid;
	glm::vec3 pos;
};

struct Cube {
	Model model;
};

#define NUM_CUBES 3
Cube g_cubes[NUM_CUBES];

void
update_init()
{
	std::optional<Render_Id> maybe_rid;
	g_cubes[0].model.pos.x = 0.0f;
	g_cubes[0].model.pos.y = 0.0f;
	g_cubes[0].model.pos.z = 0.0f;
	maybe_rid = render_add_instance("nanosuit", g_cubes[0].model.pos);
	if (maybe_rid)
		g_cubes[0].model.rid = *maybe_rid;
	g_cubes[1].model.pos.x = 0.0f;
	g_cubes[1].model.pos.y = 1.0f;
	g_cubes[1].model.pos.z = 0.0f;
	maybe_rid = render_add_instance("nanosuit", g_cubes[1].model.pos);
	if (maybe_rid)
		g_cubes[1].model.rid = *maybe_rid;
	else
		printf("a;lskdas\n");
}

void
update_sim()
{
	for (int i = 0; i < 2; ++i) {
//		g_cubes[i].model.pos.y += .1f;
//		render_update_instance(g_cubes[i].model.rid, glm::vec3(0.0f, 0.1f, 0.0f));
	}
}
