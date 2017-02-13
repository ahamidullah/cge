#ifndef __UI_H__
#define __UI_H__

#include <vector>
#include "input.h"
#include <stdint.h>

struct Color {
	float r;
	float g;
	float b;
};

struct UI_Vertex {
	Vec3f position;
	Color color;
};

struct UI_State {
	std::vector<UI_Vertex> vertices;
	float z_level;
	Vec2f cur_panel_pos;
	Vec2f cur_panel_dim;
	Vec2f cur_widget_ofs;
	intptr_t hot_widget;
	intptr_t active_widget;
	bool is_ui_active;
};

void ui_init(UI_State *);
Program_State ui_update(const Mouse &, const Vec2i &, Program_State, UI_State *);

#endif
