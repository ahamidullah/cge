#include <X11/X.h>
#include <X11/Xlib.h>
//#include <X11/extensions/Xrender.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

#define DEFINEPROC
#include "glprocs.h"
#undef DEFINEPROC

//#define stdout 1
//#define stdin 0
//#define stderr 2

enum Linux_Key_Symbols {
	PLATFORM_W_KEY = XK_w,
	PLATFORM_A_KEY = XK_a,
	PLATFORM_S_KEY = XK_s,
	PLATFORM_D_KEY = XK_d,
	PLATFORM_E_KEY = XK_e,
	PLATFORM_G_KEY = XK_g,
	PLATFORM_Q_KEY = XK_q,
	PLATFORM_R_KEY = XK_r,
	PLATFORM_F_KEY = XK_f,
};

struct Platform_Time {
	timespec time;
};

struct File_Handle {
	int descriptor;
};

#include "cge.cpp"

// TODO: Store colormap and free it on exit.
struct Platform_Context {
	Display   *display;
	Window     window;
	GLXContext gl_context;
	Atom wm_delete_window;
};

Platform_Context g_pctx;

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092

static bool ctx_error_occured = false;
static int ctx_error_handler(Display *dpy, XErrorEvent *xev)
{
	ctx_error_occured = true;
	return 0;
}

const char *
perrno()
{
	return "";
}

static bool
is_ext_supported(const char *ext_list, const char *ext)
{
	const char *start;
	const char *where, *terminator;

	// Extension names should not have spaces.
	where = strchr(ext, ' ');
	if (where || *ext == '\0')
		return false;

	for (start = ext_list;;) {
		where = strstr(start, ext);
		if (!where)
			break;

		terminator = where + strlen(ext);
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return true;
		start = terminator;
	}

	return false;
}

int
main()
{
	GLXFBConfig fbconfig;
	Vec2u screen_dim { 1600, 1200 };

	// create window
	{
		int buffer_attribs[] = {
			GLX_X_RENDERABLE    , True,
			GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
			GLX_RENDER_TYPE     , GLX_RGBA_BIT,
			GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
			GLX_RED_SIZE        , 8,
			GLX_GREEN_SIZE      , 8,
			GLX_BLUE_SIZE       , 8,
			GLX_ALPHA_SIZE      , 8,
			GLX_DEPTH_SIZE      , 24,
			GLX_STENCIL_SIZE    , 8,
			GLX_DOUBLEBUFFER    , True,
			//GLX_SAMPLE_BUFFERS  , 1,
			//GLX_SAMPLES         , 4,
			None
		};

		g_pctx.display = XOpenDisplay(NULL);
		if (!g_pctx.display)
			zabort("failed to create display");
		int xscreen = XDefaultScreen(g_pctx.display), num_fbconfigs;
		GLXFBConfig *fbconfigs = glXChooseFBConfig(g_pctx.display, xscreen, buffer_attribs, &num_fbconfigs);
		if (!fbconfigs)
			zabort("Failed to retrieve frame buffer configs");
		int best_fbc = -1, best_num_samp = -1;
		for (int i = 0; i < num_fbconfigs; ++i) {
			XVisualInfo *visual = glXGetVisualFromFBConfig(g_pctx.display, fbconfigs[i]);
			if (visual) {
				int samp_buf, samples;
				glXGetFBConfigAttrib(g_pctx.display, fbconfigs[i], GLX_SAMPLE_BUFFERS, &samp_buf);
				glXGetFBConfigAttrib(g_pctx.display, fbconfigs[i], GLX_SAMPLES, &samples);

				if (best_fbc < 0 || (samp_buf && samples > best_num_samp))
					best_fbc = i, best_num_samp = samples;
				//if (worst_fbc < 0 || !samp_buf || samples < worst_num_samp)
					//worst_fbc = i, worst_num_samp = samples;
			}
			XFree(visual);
		}
		if (best_fbc < 0)
			zabort("failed to get a frame buffer");
		fbconfig = fbconfigs[0];
		XFree(fbconfigs);

		XVisualInfo *visual = glXGetVisualFromFBConfig(g_pctx.display, fbconfig);
		Window root = DefaultRootWindow(g_pctx.display);
		XSetWindowAttributes win_attribs;
		win_attribs.colormap = XCreateColormap(g_pctx.display, root, visual->visual, AllocNone);
		win_attribs.background_pixmap = None;
		win_attribs.border_pixmap = None;
		win_attribs.border_pixel = 0;
		win_attribs.event_mask =
			StructureNotifyMask |
			FocusChangeMask |
			EnterWindowMask |
			LeaveWindowMask |
			ExposureMask |
			ButtonPressMask |
			ButtonReleaseMask |
			OwnerGrabButtonMask |
			KeyPressMask |
			KeyReleaseMask;

		int win_attribs_mask = 
			CWBackPixmap|
			CWColormap|
			CWBorderPixel|
			CWEventMask;

		assert(visual->c_class == TrueColor);
		g_pctx.window = XCreateWindow(g_pctx.display, root, 0, 0, screen_dim.x, screen_dim.y, 0, visual->depth, InputOutput, visual->visual, win_attribs_mask, &win_attribs);
		if (!g_pctx.window)
			zabort("Failed to create a window.\n");
		XFree(visual);
		XStoreName(g_pctx.display, g_pctx.window, "cge");
		XMapWindow(g_pctx.display, g_pctx.window);
		if ((g_pctx.wm_delete_window = XInternAtom(g_pctx.display, "WM_DELETE_WINDOW", 1)))
			XSetWMProtocols(g_pctx.display, g_pctx.window, &g_pctx.wm_delete_window, 1);
		else
			zerror("Unable to register WM_DELETE_WINDOW atom");
	}

	// Create OpenGL context.
	{
		int major_ver, minor_ver;
		if (!glXQueryVersion(g_pctx.display, &major_ver, &minor_ver))
			zabort("Unable to query GLX version");
		if ((major_ver == 1 && minor_ver < 3) || major_ver < 1)
			zabort("GLX version is too old");

		// Install a new error handler so that the program doesn't just exit if we fail to get a 3.1 context.
		// Note this error handler is global.  All display connections in all threads of a process use the same error handler.
		ctx_error_occured = false;
		int (*old_handler)(Display*, XErrorEvent*) = XSetErrorHandler(&ctx_error_handler);

		const char *gl_exts = glXQueryExtensionsString(g_pctx.display, XDefaultScreen(g_pctx.display));
		if (!is_ext_supported(gl_exts, "GLX_ARB_create_context"))
			zabort("OpenGL does not support glXCreateContextAttribsARB extension");

		int context_attribs[] = {
			GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
			GLX_CONTEXT_MINOR_VERSION_ARB, 1,
			//GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
			None
		};
		typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
		glXCreateContextAttribsARBProc glXCreateContextAttribsARB = NULL;
		glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");
		if (!glXCreateContextAttribsARB)
			zabort("Could not load glXCreateContextAttribsARB");
		g_pctx.gl_context = NULL;
		g_pctx.gl_context = glXCreateContextAttribsARB(g_pctx.display, fbconfig, 0, True, context_attribs);
		XSync(g_pctx.display, False);
		if (ctx_error_occured || !g_pctx.gl_context)
			zabort("Failed to create OpenGL context");
		XSetErrorHandler(old_handler);
		glXMakeCurrent(g_pctx.display, g_pctx.window, g_pctx.gl_context);
	}

	// Load OpenGL functions.
	#define LOADPROC
	#include "glprocs.h"
	#undef LOADPROC

	main_loop(screen_dim);
	platform_exit();
	return 0;
}

void
platform_exit()
{
	glXMakeCurrent(g_pctx.display, None, NULL);
	glXDestroyContext(g_pctx.display, g_pctx.gl_context);
	XDestroyWindow(g_pctx.display, g_pctx.window);
	XCloseDisplay(g_pctx.display);
	_exit(0);
}

void input_key_down(Keyboard *, unsigned);

inline unsigned
platform_keysym_to_scancode(Key_Symbol ks)
{
	return XKeysymToKeycode(g_pctx.display, ks);
}

void
platform_update_mouse(Mouse *m)
{
	Window root, child;
	Vec2i root_pos, mpos;
	unsigned mbuttons_and_key_modifiers;
	XQueryPointer(g_pctx.display, g_pctx.window, &root, &child, &root_pos.x, &root_pos.y, &mpos.x, &mpos.y, &mbuttons_and_key_modifiers);
	m->motion = mpos - m->pos;
	m->pos = mpos;
}

void
platform_handle_events(Input *in)
{
	// TODO: Set up an auto-pause when we lose focus?
	XEvent xev;
	while (XPending(g_pctx.display)) {
		XNextEvent(g_pctx.display, &xev);
		switch(xev.type) {
		case KeyPress: {
			input_key_down(&in->keyboard, xev.xkey.keycode);
		} break;
		case KeyRelease: {
			input_key_up(&in->keyboard, xev.xkey.keycode);
		} break;
		case FocusIn: {
			platform_update_mouse(&in->mouse); // Reset mouse position so the view doesn't "jump" when we regain focus.
		} break;
		case ClientMessage:
			if ((Atom)xev.xclient.data.l[0] == g_pctx.wm_delete_window)
				platform_exit();
		}
	}

	Window active;
	int revert_to;
	XGetInputFocus(g_pctx.display, &active, &revert_to);
	// TODO: Stop updating the camera if the mouse pointer isn't inside our window?
	if (active == g_pctx.window)
		platform_update_mouse(&in->mouse);
}

void
platform_swap_buffers()
{
	glXSwapBuffers(g_pctx.display, g_pctx.window);
}

void
platform_debug_print(size_t nbytes, const char* buf)
{
	platform_write({1}, nbytes, buf);
}

// 
// TODO: Signal IO errors.
//

// TODO: Handle modes.
File_Handle
platform_open_file(const char *path, const char *mode)
{
	File_Handle fh;
	fh.descriptor = open(path, O_RDONLY);
	if (fh.descriptor < 0) {
		zerror("could not open file %s\n", path);
		fh.descriptor = -1;
		return fh;
	}
	return fh;
}

void
platform_close_file(File_Handle fh)
{
	close(fh.descriptor);
}

void
platform_read(File_Handle fh, size_t read_nbytes, void *buf)
{
	off_t tot_read = 0, cur_read = 0;
	char *pos = (char *)buf;
	do {
		cur_read = read(fh.descriptor, pos, (read_nbytes - tot_read));
		tot_read += cur_read;
		pos += cur_read;
	} while (tot_read < read_nbytes && cur_read != 0);
	if (tot_read != read_nbytes)
		zerror("could not read from file %s.", perrno());
}

void
platform_write(File_Handle fh, size_t n, const void *buf)
{
	ssize_t tot_writ = 0, cur_writ = 0;
	const char *pos = (char *)buf;
	do {
		cur_writ = write(fh.descriptor, pos, (n - tot_writ));
		tot_writ += cur_writ;
		pos += cur_writ;
	} while (tot_writ < n && cur_writ != 0);
	if (tot_writ != n) {
		zerror("could not write to file. %s.", perrno());
	}
}

// TODO: Use a custom offset type?
void
platform_file_seek(File_Handle fh, unsigned offset)
{
	assert(lseek(fh.descriptor, offset, SEEK_SET) != (off_t) -1);
}

char *
platform_read_entire_file(const char *path, Memory_Arena *ma)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		zerror("could not load file %s", path);
		return NULL;
	}
	off_t len = lseek(fd, 0, SEEK_END);
	if (len < 0) {
		zerror("could not get %s file length. %s.\n", path, perrno());
		return NULL;
	}
	lseek(fd, 0, SEEK_SET);
	char *buf = mem_alloc_array(char, (len+1), ma);
	// read may return less bytes than requested, so we have to loop.
	off_t tot_read = 0, cur_read = 0;
	char *pos = buf;
	do {
		cur_read = read(fd, pos, (len - tot_read));
		tot_read += cur_read;
		pos += cur_read;
	} while (tot_read < len && cur_read != 0);
	if (tot_read != len) {
		zerror("could not read file %s.", path, perrno());
		return NULL;
	}
	buf[len] = '\0';
	close(fd);
	return buf;
}

char *
platform_get_memory(size_t len)
{
	void *m = mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (m == (void *)-1)
		zabort("failed to get memory from platform.");
	return (char *)m;
}

void
platform_free_memory(void *m, size_t len)
{
	int ret = munmap(m, len);
	if (ret == -1)
		zabort("failed to free memory.");
}

size_t
platform_get_page_size()
{
	return sysconf(_SC_PAGESIZE);
}

inline Platform_Time
platform_get_time()
{
	Platform_Time t;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t.time);
	return t;
}

// Time in milliseconds.
inline long
platform_time_diff(Platform_Time start, Platform_Time end, unsigned resolution)
{
	return (end.time.tv_nsec - start.time.tv_nsec) / resolution;
}

