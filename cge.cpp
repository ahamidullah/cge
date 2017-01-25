#include <stdio.h>
#include "render.h"
#include "zlib.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>

struct vec2 {
	int x;
	int y;
};

struct Mouse {
	vec2 pos;
	vec2 relative_pos;
	GLfloat sensitivity;
	bool buttons[3];
};

struct Keyboard {
	bool keys[256];
	bool keys_toggle[256];
};

void
key_down(Keyboard *kb, const SDL_KeyboardEvent& e)
{
	kb->keys[e.keysym.scancode] = true;
	if (!e.repeat)
		kb->keys_toggle[e.keysym.scancode] = true;
}

void
key_up(Keyboard *kb, const SDL_KeyboardEvent& e)
{
	kb->keys[e.keysym.scancode] = false;
	kb->keys_toggle[e.keysym.scancode] = false;
}

bool
is_key_down(const Keyboard *kb, const SDL_Keycode k)
{
	return kb->keys[SDL_GetScancodeFromKey(k)];
}

// has the key moved from up to down? if so, unset it in keys_toggle
// if multiple people people are looking for the same key press, only the first will get it -- bad
// but I don't want to use callbacks so this will do for now
bool
was_key_pressed(Keyboard *kb, const SDL_Keycode k)
{
	SDL_Scancode sc = SDL_GetScancodeFromKey(k);
	if (kb->keys_toggle[sc]) {
		kb->keys_toggle[sc] = false;
		return true;
	}
	return false;
}

glm::vec3
calc_front(GLfloat pitch, GLfloat yaw)
{
	glm::vec3 new_front;
	new_front.x = cos(glm::radians(pitch)) * cos(glm::radians(yaw));
	new_front.y = sin(glm::radians(pitch));
	new_front.z = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
	return glm::normalize(new_front);
}

void
update_camera(const Mouse& m, Keyboard *kb, Camera *cam)
{
	static Camera saved_cam = { -45.0f, 45.0f, 0.2f, glm::vec3(0.0f, 10.0f,  -5.0f), calc_front(-45.0f, 45.0f), glm::vec3(0.0f, 1.0f,  0.0f), 45.0f, 0.1f, 100.0f };
	static bool is_first_person = true;

	if (was_key_pressed(kb, SDLK_g)) {
		Camera temp = *cam;
		*cam = saved_cam;
		saved_cam = temp;
		is_first_person = !is_first_person;
	}

	if (is_first_person) {
		if (m.relative_pos.x != 0 || m.relative_pos.y != 0) {
			//cam->yaw += (m.pos.x - m.last_pos.x) * m.sensitivity;
			//cam->pitch += (m.last_pos.y - m.pos.y) * m.sensitivity; //reversed because y coord range from top to bottom
			cam->yaw += m.relative_pos.x * m.sensitivity;
			cam->pitch -= m.relative_pos.y * m.sensitivity; //reversed because y coord range from top to bottom
			if (cam->pitch > 89.0f) {
				cam->pitch = 89.0f;
			}
			if (cam->pitch < -89.0f) {
				cam->pitch = -89.0f;
			}
			cam->front = calc_front(cam->pitch, cam->yaw);
		}
		if(is_key_down(kb, SDLK_w))
			cam->pos += cam->speed * cam->front;
		if(is_key_down(kb, SDLK_s))
			cam->pos -= cam->speed * cam->front;
		if(is_key_down(kb, SDLK_a))
			cam->pos -= glm::normalize(glm::cross(cam->front, cam->up)) * cam->speed;
		if(is_key_down(kb, SDLK_d))
			cam->pos += glm::normalize(glm::cross(cam->front, cam->up)) * cam->speed;
		if(is_key_down(kb, SDLK_q))
			cam->pos += cam->speed * cam->up;
		if(is_key_down(kb, SDLK_e))
			cam->pos -= cam->speed * cam->up;
	} else {
		if(is_key_down(kb, SDLK_w))
			cam->pos += cam->speed * glm::vec3(cam->front.x, 0.0f, cam->front.z);
		if(is_key_down(kb, SDLK_s))
			cam->pos -= cam->speed * glm::vec3(cam->front.x, 0.0f, cam->front.z);
		if(is_key_down(kb, SDLK_a))
			cam->pos -= cam->speed * glm::cross(glm::vec3(cam->front.x, 0.0f, cam->front.z), glm::vec3(0.0f, 1.0f, 0.0f));
		if(is_key_down(kb, SDLK_d))
			cam->pos += cam->speed * glm::cross(glm::vec3(cam->front.x, 0.0f, cam->front.z), glm::vec3(0.0f, 1.0f, 0.0f));
		if(is_key_down(kb, SDLK_r))
			cam->pos += cam->speed * cam->front;
		if(is_key_down(kb, SDLK_f))
			cam->pos -= cam->speed * cam->front;
		if(is_key_down(kb, SDLK_q)) {
			cam->yaw -= 5.0f;
			cam->front = calc_front(cam->pitch, cam->yaw);
		}
		if(is_key_down(kb, SDLK_e)) {
			cam->yaw += 5.0f;
			cam->front = calc_front(cam->pitch, cam->yaw);
		}
	}
}

/*
void
recenter_mouse(Mouse *m)
{
	//SDL_GetMouseState(&m->pos.x, &m->pos.y);
	//m->last_pos = m->pos;
}
*/

enum program_state {
	run_state,
	pause_state,
	exit_state,
};

program_state
process_input(Keyboard *kb, Mouse *m, const Screen &scr, const Camera &cam)
{
	SDL_Event e;
	while(SDL_PollEvent(&e) != 0) {
		switch (e.type) {
			case SDL_QUIT:
				return exit_state;
			case SDL_KEYDOWN:
				key_down(kb, e.key);
				break;
			case SDL_KEYUP:
				key_up(kb, e.key);
				break;
			case SDL_MOUSEMOTION:
				break;
			case SDL_MOUSEBUTTONDOWN: 
			{
				auto pos = raycast_plane(glm::vec2(e.button.x, e.button.y), glm::vec3(0.0f, 1.0f, 0.0f), cam.pos, 0.0f, scr);
				if (pos)
					mk_point_light(*pos);
				break;
			}
			case SDL_MOUSEBUTTONUP:
				break;
		}
	}
	SDL_GetRelativeMouseState(&m->relative_pos.x, &m->relative_pos.y);
	SDL_GetMouseState(&m->pos.x, &m->pos.y);
	return run_state;
}

int
main()
{
	constexpr int TICKS_PER_SECOND =  25;
	constexpr int SKIP_TICKS = 1000/TICKS_PER_SECOND;
	constexpr int MAX_FRAMESKIP = 5;

	SDL_SetRelativeMouseMode(SDL_TRUE);
	Keyboard kb = { {0}, {0} };
	Camera cam = { 0.0f, 0.0f, 0.1f, glm::vec3(0.0f, 0.0f,  0.0f), calc_front(0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 45.0f, 0.1f, 100.0f };
	Mouse mouse = { {400, 300}, {400, 300}, 0.1f, {false, false, false} };
	Screen screen = { 800, 600 };
	SDL_Window *window;

	// sdl init
	{
		int winflags = (SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
		int imgflags = (IMG_INIT_PNG | IMG_INIT_JPG);
	
		if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
			zabort("SDL could not initialize video! SDL Error: %s\n", SDL_GetError());
		}

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		if (!(window = SDL_CreateWindow("cge", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen.w, screen.h, winflags))) {
			zabort("SDL could not create a window! SDL Error: %s\n", SDL_GetError());
		}
		if (!(IMG_Init(imgflags) & imgflags)) {
			zabort("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
		}
		if (!SDL_GL_CreateContext(window)) {
			zabort("SDL could not create an OpenGL context! SDL Error: %s\n", SDL_GetError());
		}
	}

	render_init(cam, screen);

	program_state state = run_state;
	int num_updates = 0;
	unsigned next_tick = SDL_GetTicks();

	while (state != exit_state) {
		switch (state) {
			case run_state: {
				num_updates = 0;
				while (next_tick < SDL_GetTicks() && num_updates < MAX_FRAMESKIP) {
					state = process_input(&kb, &mouse, screen, cam);
					update_camera(mouse, &kb, &cam);
					render_update_view(cam);
					next_tick += SKIP_TICKS;
					++num_updates;
				}
				render(cam);
				SDL_GL_SwapWindow(window);
				break;
			}
			case pause_state: {
				break;
			}
			case exit_state: {
				break;
			}
		}
	}
	SDL_DestroyWindow(window);
//	SDL_GL_DeleteContext(context);
	IMG_Quit();
	SDL_Quit();
	return 0;
}
