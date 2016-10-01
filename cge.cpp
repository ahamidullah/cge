#include <stdio.h>
#include <tuple>
#include "render.h"
#include "zlib.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>

enum {
	SCREEN_WIDTH = 800,
	SCREEN_HEIGHT = 600,
};

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
key_down(Keyboard *kb_state, const SDL_KeyboardEvent& e)
{
	kb_state->keys[e.keysym.scancode] = true;
	if (!e.repeat)
		kb_state->keys_toggle[e.keysym.scancode] = true;
}

void
key_up(Keyboard *kb_state, const SDL_KeyboardEvent& e)
{
	kb_state->keys[e.keysym.scancode] = false;
	kb_state->keys_toggle[e.keysym.scancode] = false;
}

bool
is_key_down(const Keyboard *kb_state, const SDL_Keycode k)
{
	return kb_state->keys[SDL_GetScancodeFromKey(k)];
}

// has the key moved from up to down? if so, unset it in keys_toggle
// if multiple people people are looking for the same key press, only the first will get it -- bad
// but I don't want to use callbacks so this will do for now
bool
was_key_pressed(Keyboard *kb_state, const SDL_Keycode k)
{
	SDL_Scancode sc = SDL_GetScancodeFromKey(k);
	if (kb_state->keys_toggle[sc]) {
		kb_state->keys_toggle[sc] = false;
		return true;
	}
	return false;
}

void
update_mouse_pos(Mouse *m)
{
	SDL_GetRelativeMouseState(&m->relative_pos.x, &m->relative_pos.y);
	SDL_GetMouseState(&m->pos.x, &m->pos.y);
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
	static Camera saved_cam = { -45.0f, 45.0f, 0.2f, glm::vec3(0.0f, 10.0f,  -5.0f), calc_front(-45.0f, 45.0f), glm::vec3(0.0f, 1.0f,  0.0f) };
	static bool is_first_person = true;

	if (was_key_pressed(kb, SDLK_g)) {
		Camera temp = *cam;
		*cam = saved_cam;
		saved_cam = temp;
		is_first_person = !is_first_person;
	}

	if (is_first_person) {
		//if (m.pos.x != m.last_pos.x || m.pos.y != m.last_pos.y) {
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
		//}
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

void
recenter_mouse(Mouse *m)
{
	//SDL_GetMouseState(&m->pos.x, &m->pos.y);
	//m->last_pos = m->pos;
}

std::tuple<SDL_Window*, SDL_GLContext>
init_sdl()
{
	SDL_Window *win;
	SDL_GLContext context;

	int winflags = (SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	int imgflags = (IMG_INIT_PNG | IMG_INIT_JPG);

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		z_abort("SDL could not initialize video! SDL Error: %s\n", SDL_GetError());
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	if (!(win = SDL_CreateWindow("cge", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, winflags))) {
		z_abort("SDL could not create a window! SDL Error: %s\n", SDL_GetError());
	}
	//initialize PNG loading
	if (!(IMG_Init(imgflags) & imgflags)) {
		z_abort("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
	}
	if (!(context = SDL_GL_CreateContext(win))) {
		z_abort("SDL could not create an OpenGL context! SDL Error: %s\n", SDL_GetError());
	}
	return std::make_tuple(win, context);
}

enum game_state {
	game_play,
	game_pause,
	game_exit,
};

game_state
process_input(Keyboard *kb, Mouse *m, const Camera &cam)
{
	SDL_Event e;
	while(SDL_PollEvent(&e) != 0) {
		switch (e.type) {
			case SDL_QUIT:
				return game_exit;
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
				float x = (2.0f * e.button.x) / SCREEN_WIDTH - 1.0f;
				float y = 1.0f - (2.0f * e.button.y) / SCREEN_HEIGHT;
				glm::vec4 ray_clip = glm::vec4(x, y, -1.0f, 1.0f);
        			glm::mat4 projection = glm::perspective(45.0f, (GLfloat)SCREEN_WIDTH / (GLfloat)SCREEN_HEIGHT, 0.1f, 100.0f);
				glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
				ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);
				glm::mat4 view = glm::lookAt(cam.pos, cam.pos + cam.front, cam.up);
				glm::vec3 ray_world = glm::normalize((glm::inverse(view) * ray_eye).xyz());
				float t = -(glm::dot(cam.pos, glm::vec3(0.0f, 1.0f, 0.0f)) / glm::dot(ray_world, glm::vec3(0.0f, 1.0f, 0.0f)));
				glm::vec3 n = glm::vec3(0.0f, 1.0f, 0.0f);
				//glm::vec3 t = (-n * cam.pos) / (n * ray_world);
				//glm::vec3 yy = (cam.pos * glm::vec3(0.0f, 1.0f, 0.0f) + cam.pos);
				//glm::vec3 xx = (ray_world * glm::vec3(0.0f, 1.0f, 0.0f));
				glm::vec3 pos = cam.pos + ray_world * t;
				make_point_light({ pos.x, pos.y, pos.z });
				printf("%f, %f, %f\n", pos.x, pos.y, pos.z);
				break;
			}
			case SDL_MOUSEBUTTONUP:
				break;
		}
	}
	update_mouse_pos(m);
	return game_play;
}

int
main()
{
	SDL_Window *window;
	SDL_GLContext context;
	std::tie(window, context) = init_sdl();

	const unsigned int TICKS_PER_SECOND =  25;
	const unsigned int SKIP_TICKS = 1000/TICKS_PER_SECOND;
	const unsigned int MAX_FRAMESKIP = 5;

	game_state state = game_play;
	unsigned int num_updates = 0;
	unsigned int next_game_tick = 0;
	//SDL_SetRelativeMouseMode(SDL_TRUE);
	Keyboard kb = { {0}, {0} };
	Camera cam = { 0.0f, 0.0f, 0.1f, glm::vec3(0.0f, 0.0f,  0.0f), calc_front(0.0f, 0.0f), glm::vec3(0.0f, 1.0f,  0.0f) };
	Mouse mouse = { {400, 300}, {400, 300}, 0.1f, {false, false, false} };

	render_init(SCREEN_WIDTH, SCREEN_HEIGHT);
	while (!(state == game_exit)) {
		switch (state) {
			case game_play: {
				num_updates = 0;
				while (next_game_tick < SDL_GetTicks() && num_updates < MAX_FRAMESKIP) {
					state = process_input(&kb, &mouse, cam);
					update_camera(mouse, &kb, &cam);
					next_game_tick += SKIP_TICKS;
					num_updates++;
				}
				render(cam);
				SDL_GL_SwapWindow(window);
				break;
			}
			case game_pause: {
				break;
			}
			case game_exit: {
				break;
			}
		}
	}
	SDL_DestroyWindow(window);
	SDL_GL_DeleteContext(context);
	IMG_Quit();
	SDL_Quit();
	return 0;
}

