void
input_key_down(Keyboard *kb, unsigned scancode)
{
	kb->keys[scancode] = true;
	if (kb->keys_toggle[scancode])
		kb->keys_toggle[scancode] = false;
	else
		kb->keys_toggle[scancode] = true;
}

void
input_key_up(Keyboard *kb, unsigned scancode)
{
	kb->keys[scancode] = false;
	kb->keys_toggle[scancode] = false;
}

bool
input_is_key_down(const Keyboard *kb, Key_Symbol ks)
{
	return kb->keys[platform_keysym_to_scancode(ks)];
}

// has the key moved from up to down? if so, unset it in keys_toggle
// if multiple people people are looking for the same key press, only the first will get it -- bad
// but I don't want to use callbacks so this will do for now
bool
input_was_key_pressed(Keyboard *kb, Key_Symbol ks)
{
	unsigned sc = platform_keysym_to_scancode(ks);
	//SDL_Scancode sc = SDL_GetScancodeFromKey(k);
	if (kb->keys_toggle[sc]) {
		kb->keys_toggle[sc] = false;
		return true;
	}
	return false;
}

/*
void
input_mbutton_down(const SDL_MouseButtonEvent &e, Mouse *m)
{
	if (e.button == SDL_BUTTON_LEFT)
		m->buttons |= mbutton_left;
	else if (e.button == SDL_BUTTON_MIDDLE)
		m->buttons |= mbutton_middle;
	else if (e.button == SDL_BUTTON_RIGHT)
		m->buttons |= mbutton_right;
}

void
input_mbutton_up(const SDL_MouseButtonEvent &e, Mouse *m)
{
	if (e.button == SDL_BUTTON_LEFT)
		m->buttons &= ~mbutton_left;
	else if (e.button == SDL_BUTTON_MIDDLE)
		m->buttons &= ~mbutton_middle;
	else if (e.button == SDL_BUTTON_RIGHT)
		m->buttons &= ~mbutton_right;
}

void
input_update_mouse(Mouse *m)
{
	SDL_GetRelativeMouseState(&m->motion.x, &m->motion.y);
	SDL_GetMouseState(&m->pos.x, &m->pos.y);
}
*/
