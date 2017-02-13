#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>

#include "render.h"
#include "update.h"
#include "zlib.h"
#include "ui.h"
#include "input.h"

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

	if (input_was_key_pressed(kb, SDLK_g)) {
		Camera temp = *cam;
		*cam = saved_cam;
		saved_cam = temp;
		is_first_person = !is_first_person;
	}

	if (is_first_person) {
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
		if(input_is_key_down(kb, SDLK_w))
			cam->pos += cam->speed * cam->front;
		if(input_is_key_down(kb, SDLK_s))
			cam->pos -= cam->speed * cam->front;
		if(input_is_key_down(kb, SDLK_a))
			cam->pos -= glm::normalize(glm::cross(cam->front, cam->up)) * cam->speed;
		if(input_is_key_down(kb, SDLK_d))
			cam->pos += glm::normalize(glm::cross(cam->front, cam->up)) * cam->speed;
		if(input_is_key_down(kb, SDLK_q))
			cam->pos += cam->speed * cam->up;
		if(input_is_key_down(kb, SDLK_e))
			cam->pos -= cam->speed * cam->up;
	} else {
		if(input_is_key_down(kb, SDLK_w))
			cam->pos += cam->speed * glm::vec3(cam->front.x, 0.0f, cam->front.z);
		if(input_is_key_down(kb, SDLK_s))
			cam->pos -= cam->speed * glm::vec3(cam->front.x, 0.0f, cam->front.z);
		if(input_is_key_down(kb, SDLK_a))
			cam->pos -= cam->speed * glm::cross(glm::vec3(cam->front.x, 0.0f, cam->front.z), glm::vec3(0.0f, 1.0f, 0.0f));
		if(input_is_key_down(kb, SDLK_d))
			cam->pos += cam->speed * glm::cross(glm::vec3(cam->front.x, 0.0f, cam->front.z), glm::vec3(0.0f, 1.0f, 0.0f));
		if(input_is_key_down(kb, SDLK_r))
			cam->pos += cam->speed * cam->front;
		if(input_is_key_down(kb, SDLK_f))
			cam->pos -= cam->speed * cam->front;
		if(input_is_key_down(kb, SDLK_q)) {
			cam->yaw -= 5.0f;
			cam->front = calc_front(cam->pitch, cam->yaw);
		}
		if(input_is_key_down(kb, SDLK_e)) {
			cam->yaw += 5.0f;
			cam->front = calc_front(cam->pitch, cam->yaw);
		}
	}
}

Program_State
handle_events(Keyboard *kb, Mouse *m, const Vec2i &screen_dim, const Camera &cam)
{
	SDL_Event e;
	while(SDL_PollEvent(&e) != 0) {
		switch (e.type) {
		case SDL_QUIT:
			return Program_State::exit;
		case SDL_KEYDOWN:
			input_key_down(kb, e.key);
			break;
		case SDL_KEYUP:
			input_key_up(kb, e.key);
			break;
		case SDL_MOUSEBUTTONDOWN: 
		{
			auto pos = raycast_plane(glm::vec2(e.button.x, e.button.y), glm::vec3(0.0f, 1.0f, 0.0f), cam.pos, 0.0f, screen_dim);
			if (pos)
				mk_point_light(*pos);
			input_mbutton_down(e.button, m);
			break;
		}
		case SDL_MOUSEBUTTONUP:
			input_mbutton_up(e.button, m);
			break;
		}
	}
	return Program_State::run;
}

int
main()
{
	//SDL_SetRelativeMouseMode(SDL_TRUE);
	Keyboard kb = { {0}, {0} };
	Camera cam = { 0.0f, 0.0f, 1.1f, glm::vec3(0.0f, 0.0f,  0.0f), calc_front(0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 45.0f, 0.1f, 100.0f };
	Mouse mouse = { {400, 300}, {400, 300}, 0.1f, 0 };
	Vec2i screen_dim = { 1000, 800 };
	UI_State ui;
	SDL_Window *window;
	SDL_GLContext context;

	// sdl init
	{
		int winflags = (SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
		int imgflags = (IMG_INIT_PNG | IMG_INIT_JPG);
	
		if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) < 0) {
			zabort("SDL could not initialize video! SDL Error: %s\n", SDL_GetError());
		}

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		if (!(window = SDL_CreateWindow("cge", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_dim.x, screen_dim.y, winflags))) {
			zabort("SDL could not create a window! SDL Error: %s\n", SDL_GetError());
		}
		if (!(IMG_Init(imgflags) & imgflags)) {
			zabort("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
		}
		if (!(context = SDL_GL_CreateContext(window))) {
			zabort("SDL could not create an OpenGL context! SDL Error: %s\n", SDL_GetError());
		}
	}

	render_init(cam, screen_dim);
	update_init();
	ui_init(&ui);

	Program_State state = Program_State::run;
	constexpr unsigned TICKS_PER_SECOND =  25;
	constexpr unsigned SKIP_TICKS = 1000/TICKS_PER_SECOND;
	constexpr unsigned MAX_FRAMESKIP = 5;
	unsigned num_updates = 0;
	unsigned next_tick = SDL_GetTicks();

	while (state != Program_State::exit) {
		switch (state) {
		case Program_State::run: {
			num_updates = 0;
			while (next_tick < SDL_GetTicks() && num_updates < MAX_FRAMESKIP) {
				state = handle_events(&kb, &mouse, screen_dim, cam);
				input_update_mouse(&mouse);
				update_camera(mouse, &kb, &cam);
				render_update_view(cam);
				update_sim();
				state = ui_update(mouse, screen_dim, state, &ui);
				next_tick += SKIP_TICKS;
				++num_updates;
			}
			render_sim();
			render_ui(ui);
			SDL_GL_SwapWindow(window);
			break;
		}
		case Program_State::pause:
			break;
		case Program_State::exit:
			break;
		}
	}
	render_quit();
	SDL_DestroyWindow(window);
	SDL_GL_DeleteContext(context);
	IMG_Quit();
	SDL_Quit();
	return 0;
}

