#ifndef __INPUT_H__
#define __INPUT_H__

//#include <SDL2/SDL.h>

#define MBUTTON_IS_DOWN(buttons, which) (buttons & (unsigned)which)
#define MBUTTON_IS_UP(buttons, which) (!MBUTTON_IS_DOWN(buttons, (unsigned)which))

// TODO: move some of this stuff to a generic shared header
enum Mouse_Buttons {
	mbutton_left = 0x1,
	mbutton_middle = 0x2,
	mbutton_right = 0x4
};

struct Mouse {
	Vec2i pos;
	Vec2i motion;
	float sensitivity;
	unsigned buttons;
};

struct Keyboard {
	bool keys[256];
	bool keys_toggle[256];
};

struct Input {
	Mouse mouse;
	Keyboard keyboard;
};

#endif
