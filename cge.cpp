#include <stdio.h>
#include <tuple>
#include "render.h"
#include "zlib.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>

enum {
	SCREEN_WIDTH = 800,
	SCREEN_HEIGHT = 600,
};

struct camera {
	GLfloat pitch;
	GLfloat yaw;
	GLfloat speed;
	glm::vec3 pos;
	glm::vec3 front;
	glm::vec3 up;
};

struct vec2 {
	s32 x;
	s32 y;
};

struct mouse {
	vec2 pos;
	vec2 last_pos;
	GLfloat sensitivity;
};


std::tuple<SDL_Window*, SDL_GLContext> init_sdl();

void
keydown(const SDL_Keycode k, bool keys[])
{
	keys[SDL_GetScancodeFromKey(k)] = true;
}

void
keyup(const SDL_Keycode k, bool keys[])
{
	keys[SDL_GetScancodeFromKey(k)] = false;
}

inline bool
iskeydown(const SDL_Keycode k, const bool keys[])
{
	return keys[SDL_GetScancodeFromKey(k)];
}

void
mouse_move(const vec2 new_pos, mouse *m)
{
	m->last_pos.x = m->pos.x;
	m->last_pos.y = m->pos.y;
	m->pos.x = new_pos.x;
	m->pos.y = new_pos.y;
}

void
update_camera(const mouse& m, const bool keys[], camera *cam)
{
	if (m.pos.x != m.last_pos.x || m.pos.y != m.last_pos.y) {
		const GLfloat xoffset = (m.pos.x - m.last_pos.x) * m.sensitivity;
		const GLfloat yoffset = (m.last_pos.y - m.pos.y) * m.sensitivity; //reversed because y coord range from top to bottom;
		cam->pitch += yoffset;
		cam->yaw += xoffset;
		if (cam->pitch > 89.0f) {
			cam->pitch = 89.0f;
		}
		if (cam->pitch < -89.0f) {
			cam->pitch = -89.0f;
		}
		glm::vec3 new_front;
		new_front.x = cos(glm::radians(cam->pitch)) * cos(glm::radians(cam->yaw));
		new_front.y = sin(glm::radians(cam->pitch));
		new_front.z = cos(glm::radians(cam->pitch)) * sin(glm::radians(cam->yaw));
		cam->front = glm::normalize(new_front);
	}
	if(iskeydown(SDLK_w, keys)) {
		cam->pos += cam->speed * cam->front;
	}
	if(iskeydown(SDLK_s, keys)) {
		cam->pos -= cam->speed * cam->front;
	}
	if(iskeydown(SDLK_a, keys)) {
		cam->pos -= glm::normalize(glm::cross(cam->front, cam->up)) * cam->speed;
	}
	if(iskeydown(SDLK_d, keys)) {
		cam->pos += glm::normalize(glm::cross(cam->front, cam->up)) * cam->speed;
	}
}

void
recenter_mouse(const vec2 new_pos, mouse *m)
{
	m->pos = new_pos;
	m->last_pos = new_pos;
}

int
main()
{

	SDL_Window *window;
	SDL_GLContext context;
	bool running = true;
	SDL_Event e;
	bool keys[256] = {0};
	camera camera = { 0.0f, -90.0f, 0.05f, glm::vec3(0.0f, 0.0f,  3.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f,  0.0f) };
	mouse mouse = { {400, 300}, {400, 300}, 0.1f};

	std::tie(window, context) = init_sdl();
	render::init(SCREEN_WIDTH, SCREEN_HEIGHT);

	while(running) {
		vec2 new_mouse_pos = mouse.pos;

		while(SDL_PollEvent(&e) != 0) {
			SDL_GetMouseState(&new_mouse_pos.x, &new_mouse_pos.y);

			switch (e.type) {
			case SDL_QUIT:
				running = false;
				break;
			case SDL_KEYDOWN:
				keydown(e.key.keysym.sym, keys);
				break;
			case SDL_KEYUP:
				keyup(e.key.keysym.sym, keys);
				break;
			case SDL_WINDOWEVENT:
				if (SDL_WINDOWEVENT_FOCUS_GAINED) {
					recenter_mouse(new_mouse_pos, &mouse);
				}
			}
		}
		mouse_move(new_mouse_pos, &mouse);
		update_camera(mouse, keys, &camera);
		glm::mat4 view;
		view = glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);
		render::render(view);
		SDL_GL_SwapWindow(window);
	}
	return 0;
}

std::tuple<SDL_Window*, SDL_GLContext>
init_sdl()
{
	SDL_Window *win;
	SDL_GLContext context;

	int winflags = (SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	int imgflags = (IMG_INIT_PNG | IMG_INIT_JPG);

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		zlib::ABORT("SDL could not initialize video! SDL Error: %s\n", SDL_GetError());
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	if (!(win = SDL_CreateWindow("cge", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, winflags))) {
		zlib::ABORT("SDL could not create a window! SDL Error: %s\n", SDL_GetError());
	}
	//initialize PNG loading
	if (!(IMG_Init(imgflags) & imgflags)) {
		zlib::ABORT("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
	}
	if (!(context = SDL_GL_CreateContext(win))) {
		zlib::ABORT("SDL could not create an OpenGL context! SDL Error: %s\n", SDL_GetError());
	}
	return std::make_tuple(win, context);
}
