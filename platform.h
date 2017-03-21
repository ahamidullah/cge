#ifndef __PLATFORM_H__
#define __PLATFORM_H__

enum Key_Symbol {
	A_KEY = PLATFORM_A_KEY,
	D_KEY = PLATFORM_D_KEY,
	E_KEY = PLATFORM_E_KEY,
	F_KEY = PLATFORM_F_KEY,
	G_KEY = PLATFORM_G_KEY,
	Q_KEY = PLATFORM_Q_KEY,
	R_KEY = PLATFORM_R_KEY,
	S_KEY = PLATFORM_S_KEY,
	W_KEY = PLATFORM_W_KEY,
};

enum Mouse_Button {
	MBUTTON_1 = PLATFORM_MBUTTON_1,
};

struct File_Handle;
struct Platform_Time;

void platform_update_mouse_pos(Mouse *);
unsigned platform_keysym_to_scancode(Key_Symbol);
void platform_exit();
void platform_handle_events(Input *);
void platform_swap_buffers();
void platform_debug_print(size_t, const char *);
File_Handle platform_open_file(const char *path, const char *modes);
void platform_close_file(File_Handle);
void platform_file_seek(File_Handle, unsigned);
void platform_read(File_Handle, size_t, void *);
void platform_write(File_Handle, size_t, const void *);
char *platform_read_entire_file(const char *, Memory_Arena *);
char *platform_get_memory(size_t);
void platform_free_memory(void *, size_t);
size_t platform_get_page_size();
Platform_Time platform_get_time();
long platform_time_diff(Platform_Time, Platform_Time, unsigned);

#endif
