#include <stdio.h>
#include "render.h"
#include "zlib.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>

enum {
	SCREEN_WIDTH = 800,
	SCREEN_HEIGHT = 600,
};
static SDL_Window *g_window;
static SDL_GLContext g_context;

namespace cge {
void init();
}
void init_sdl();

int
main(int argc, char** argv)
{
	bool running = true;
	SDL_Event e;

	cge::init();
	while(running) {
		while(SDL_PollEvent(&e) != 0) {
			switch (e.type) {
				case SDL_QUIT:
				running = false;
				break;
			}
		}
		render::render();
		SDL_GL_SwapWindow(g_window);
	}
	return 0;
}

namespace cge {

void
init()
{
	init_sdl();
	render::init(SCREEN_WIDTH, SCREEN_HEIGHT);
}

}

void
init_sdl()
{
	int winflags = (SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	int imgflags = (IMG_INIT_PNG | IMG_INIT_JPG);

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		zlib::ABORT("SDL could not initialize video! SDL_Error: %s\n", SDL_GetError());
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	if (!(g_window = SDL_CreateWindow("cge", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, winflags))) {
		zlib::ABORT("SDL could not create a window! SDL_Error: %s\n", SDL_GetError());
	}
	//initialize PNG loading
	if (!(IMG_Init(imgflags) & imgflags)) {
		zlib::ABORT("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
	}
	if (!(g_context = SDL_GL_CreateContext(g_window))) {
		zlib::ABORT("SDL could not create an OpenGL context! SDL_Error: %s\n", SDL_GetError());
	}
}
