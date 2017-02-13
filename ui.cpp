#include "ui.h"

#define DEFAULT_PANEL_W 10.0f
#define DEFAULT_PANEL_H 20.0f
#define ONE_Z_LEVEL 0.01f

void
ui_init(UI_State *ui)
{
	ui->vertices.resize(100); // temp
	ui->z_level = 0.0f;
	ui->hot_widget = 0;
	ui->active_widget = 0;
	ui->is_ui_active = false;
}

void
push_quad(float x, float y, float z, float w, float h, Color color, std::vector<UI_Vertex> *v)
{
	v->push_back({{x, y, z}, color});
	v->push_back({{x, y+h, z}, color});
	v->push_back({{x+w, y+h, z}, color});
	v->push_back({{x+w, y+h, z}, color});
	v->push_back({{x+w, y, z}, color});
	v->push_back({{x, y, z}, color});
}

bool
inside(const Vec2f &mpos, const Vec2f &box_pos, const Vec2f &box_dim)
{
	if (mpos.x > box_pos.x && mpos.x < box_pos.x + box_dim.x &&
	    mpos.y > box_pos.y && mpos.y < box_pos.y + box_dim.y)
		return true;
	return false;
}

bool
button(const char *text, const Vec2f &mpos, const char mbuttons, UI_State *ui)
{
	Color color = {0.0f, 1.0f, 0.0f};
	float w = 10.0f, h = 8.0f;
	bool ret = false, is_mouse_inside = inside(mpos, ui->cur_widget_ofs, {w,h});
	intptr_t our_id = (intptr_t)text;

	if (MBUTTON_IS_UP(mbuttons, mbutton_left) && is_mouse_inside) {
		ui->hot_widget = (intptr_t)text;
		color = {0.0f, 1.0f, 1.0f};
	}
	if (MBUTTON_IS_DOWN(mbuttons, mbutton_left) && is_mouse_inside && ui->hot_widget == our_id) {
		ui->active_widget = our_id;
		color = {1.0f, 1.0f, 1.0f};
	}
	if (MBUTTON_IS_UP(mbuttons, mbutton_left) && is_mouse_inside && ui->active_widget == our_id)
		ret = true;
	push_quad(ui->cur_widget_ofs.x, ui->cur_widget_ofs.y, ui->z_level, w, h, color, &ui->vertices);
	ui->cur_widget_ofs.y += h+1;
	return ret;
}

void
begin_panel(float x, float y, UI_State *ui)
{
	ui->z_level += ONE_Z_LEVEL;
	ui->cur_panel_pos.x = x;
	ui->cur_panel_pos.y = y;
	ui->cur_widget_ofs.x = x;
	ui->cur_widget_ofs.y = y;
	ui->cur_panel_dim.x = DEFAULT_PANEL_W;
	ui->cur_panel_dim.y = DEFAULT_PANEL_H;
}

void
end_panel(UI_State *ui)
{
	Color color = {0.0f, 0.0f, 1.0f};
	ui->z_level -= ONE_Z_LEVEL;
	if (ui->cur_widget_ofs.y > ui->cur_panel_dim.y)
		ui->cur_panel_dim.y = ui->cur_widget_ofs.y;
	push_quad(ui->cur_panel_pos.x, ui->cur_panel_pos.y, ui->z_level, ui->cur_panel_dim.x, ui->cur_panel_dim.y, color, &ui->vertices);
}

Program_State
ui_update(const Mouse &m, const Vec2i &screen_dim, Program_State cur_state, UI_State *ui)
{
	Program_State post_state = cur_state;
	ui->vertices.clear();
	ui->z_level = 0.0f;
	Vec2f normalized_mpos = { (float)m.pos.x/screen_dim.x*100.0f, (float)m.pos.y/screen_dim.y*100.0f };

	begin_panel(0.0f, 0.0f, ui);
	if (button("exit", normalized_mpos, m.buttons, ui))
		post_state = Program_State::exit;
	if (button("bar", normalized_mpos, m.buttons, ui))
		printf("bar pressed\n");
	if (button("baz", normalized_mpos, m.buttons, ui))
		printf("baz pressed\n");
	if (button("wub", normalized_mpos, m.buttons, ui))
		printf("wub pressed\n");
	end_panel(ui);

	if (MBUTTON_IS_UP(m.buttons, mbutton_left))
		ui->active_widget = 0;
	return post_state;
}
