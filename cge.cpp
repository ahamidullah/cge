#include <stdint.h>
#include <assert.h>
#include <stdarg.h>

#include "asset_ids.h"
#include "math.h"
#include "lib.h"
#include "input.h"
#include "memory.h"
#include "platform.h"
#include "memory.cpp"
#include "lib.cpp"
#include "render.cpp"
#include "input.cpp"

enum struct Program_State {
	run = 0,
	pause,
	exit
};

Vec3f
calc_front(float pitch, float yaw)
{
	Vec3f new_front;
	new_front.x = _cos(pitch * M_DEG_TO_RAD) * _cos(yaw * M_DEG_TO_RAD);
	new_front.y = _sin(pitch * M_DEG_TO_RAD);
	new_front.z = _cos(pitch * M_DEG_TO_RAD) * _sin(yaw * M_DEG_TO_RAD);
	return normalize(new_front);
}

void
update_camera(const Mouse& m, Keyboard *kb, Camera *cam)
{
/*
	static Camera saved_cam = { -45.0f, 45.0f, 0.2f, glm::vec3(0.0f, 10.0f,  -5.0f), calc_front(-45.0f, 45.0f), glm::vec3(0.0f, 1.0f,  0.0f), 45.0f, 0.1f, 100.0f };
	static bool is_first_person = true;

	if (input_was_key_pressed(kb, G_KEY)) {
		Camera temp = *cam;
		*cam = saved_cam;
		saved_cam = temp;
		is_first_person = !is_first_person;
	}
*/
	//if (is_first_person) {
	if (true) {
		if (m.motion.x != 0 || m.motion.y != 0) {
			cam->yaw += m.motion.x * m.sensitivity;
			cam->pitch -= m.motion.y * m.sensitivity; //reversed because y coord range from top to bottom
			if (cam->pitch > 89.0f) {
				cam->pitch = 89.0f;
			}
			if (cam->pitch < -89.0f) {
				cam->pitch = -89.0f;
			}
			cam->front = calc_front(cam->pitch, cam->yaw);
		}
		if(input_is_key_down(kb, W_KEY))
			cam->pos += cam->speed * cam->front;
		if(input_is_key_down(kb, S_KEY))
			cam->pos -= cam->speed * cam->front;
		if(input_is_key_down(kb, A_KEY))
			cam->pos -= cam->speed * normalize(cross_product(cam->front, cam->up));
		if(input_is_key_down(kb, D_KEY))
			cam->pos += cam->speed * normalize(cross_product(cam->front, cam->up));
		if(input_is_key_down(kb, Q_KEY))
			cam->pos += cam->speed * cam->up;
		if(input_is_key_down(kb, E_KEY))
			cam->pos -= cam->speed * cam->up;
	} else {/*
		if(input_is_key_down(kb, W_KEY))
			cam->pos += cam->speed * glm::vec3(cam->front.x, 0.0f, cam->front.z);
		if(input_is_key_down(kb, S_KEY))
			cam->pos -= cam->speed * glm::vec3(cam->front.x, 0.0f, cam->front.z);
		if(input_is_key_down(kb, A_KEY))
			cam->pos -= cam->speed * glm::cross(glm::vec3(cam->front.x, 0.0f, cam->front.z), glm::vec3(0.0f, 1.0f, 0.0f));
		if(input_is_key_down(kb, D_KEY))
			cam->pos += cam->speed * glm::cross(glm::vec3(cam->front.x, 0.0f, cam->front.z), glm::vec3(0.0f, 1.0f, 0.0f));
		if(input_is_key_down(kb, R_KEY))
			cam->pos += cam->speed * cam->front;
		if(input_is_key_down(kb, F_KEY))
			cam->pos -= cam->speed * cam->front;
		if(input_is_key_down(kb, Q_KEY)) {
			cam->yaw -= 5.0f;
			cam->front = calc_front(cam->pitch, cam->yaw);
		}
		if(input_is_key_down(kb, E_KEY)) {
			cam->yaw += 5.0f;
			cam->front = calc_front(cam->pitch, cam->yaw);
		}
		*/
	}
}

void
main_loop(Vec2u screen_dim)
{
	Camera cam = { 0.0f, 0.0f, 0.5f, { 0.0f, 0.0f,  0.0f }, calc_front(0.0f, 0.0f), { 0.0f, 1.0f, 0.0f }, 45.0f, 0.1f, 100.0f };
	//cam.pos -= 50.0f*cam.front;
	//Camera cam = { -45.0f, 45.0f, 0.2f, glm::vec3(0.0f, 10.0f,  -5.0f), calc_front(-45.0f, 45.0f), glm::vec3(0.0f, 1.0f,  0.0f), 45.0f, 0.1f, 100.0f };
	Input input;
	input.mouse = { {screen_dim.x/2, screen_dim.y/2}, {0, 0}, 0.1f, 0 };
	input.keyboard = { {0}, {0} };

	Program_State state = Program_State::run;
	constexpr unsigned TICKS_PER_SECOND =  25;
	constexpr unsigned SKIP_TICKS = 1000/TICKS_PER_SECOND;
	constexpr unsigned MAX_FRAMESKIP = 5;
	unsigned num_updates = 0;
	//unsigned next_tick = SDL_GetTicks();
	render_init(cam, screen_dim);
	render_add_instance(NANOSUIT_MODEL, { 0.0f, 0.0f, 0.0f });
	render_add_instance(NANOSUIT_MODEL, { 5.0f, 0.0f, 0.0f });

	while (state != Program_State::exit) {
		switch (state) {
		case Program_State::run: {
			num_updates = 0;
			//while (next_tick < SDL_GetTicks() && num_updates < MAX_FRAMESKIP) {
			while (1) {
				platform_handle_events(&input);
				//state = handle_events(&kb, &mouse, screen_dim, cam);
				//input_update_mouse(&mouse);
				//update_sim();
				//state = ui_update(mouse, screen_dim, state, &ui);
				//next_tick += SKIP_TICKS;
				++num_updates;
				update_camera(input.mouse, &input.keyboard, &cam);
				render_update_view(cam);
				render_sim();
				platform_swap_buffers();
			}
			//render_sim();
			//render_ui(ui);
			//SDL_GL_SwapWindow(window);
			break;
		}
		case Program_State::pause:
			break;
		case Program_State::exit:
			break;
		}
	}
}
