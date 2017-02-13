#ifndef __INPUT_H__
#define __INPUT_H__

#include <SDL2/SDL.h>

#define MBUTTON_IS_DOWN(buttons, which) (buttons & which)
#define MBUTTON_IS_UP(buttons, which) (!MBUTTON_IS_DOWN(buttons, which))

// TODO: move some of this stuff to a generic shared header
enum struct Program_State {
	run = 0,
	pause,
	exit
};

struct Vec2i {
	int x;
	int y;
};

struct Vec2f {
	float x;
	float y;
};

struct Vec3f {
	float x;
	float y;
	float z;
};

enum Mouse_Buttons {
	mbutton_left = 0x1,
	mbutton_middle = 0x2,
	mbutton_right = 0x4
};

struct Mouse {
	Vec2i pos;
	Vec2i motion;
	float sensitivity;
	unsigned char buttons;
};

struct Keyboard {
	bool keys[256];
	bool keys_toggle[256];
};

void input_key_up(Keyboard *, const SDL_KeyboardEvent &);
void input_key_down(Keyboard *, const SDL_KeyboardEvent &);
bool input_is_key_down(const Keyboard *, const SDL_Keycode);
bool input_was_key_pressed(Keyboard *, const SDL_Keycode);
void input_mbutton_up(const SDL_MouseButtonEvent &, Mouse *);
void input_mbutton_down(const SDL_MouseButtonEvent &, Mouse *);
void input_update_mouse(Mouse *);

#endif
