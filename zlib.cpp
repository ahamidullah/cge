#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include "zlib.h"

void
error(const char *filename, int linenum, const char *funcname, bool abort, const char *fmt, ...)
{
	va_list argptr;
	char msg[1024]; // temp
	va_start(argptr, fmt);
	vsprintf(msg, fmt, argptr);
	va_end(argptr);
	fprintf(stderr, "Error: %s:%d in %s - %s\n", filename, linenum, funcname, msg);
	if (abort)
		exit(1);
}

char *
load_file(const char *fname, const char *mode)
{
	SDL_RWops *fp = SDL_RWFromFile(fname, mode);
	if (!fp) {
		zerror("could not load file %s!\n", fname);
		return NULL;
	}
	Sint64 len = SDL_RWsize(fp);
	if (len < 0) {
		zerror("could not get %s file length! SDL Error: %s\n", fname, SDL_GetError());
		return NULL;
	}
	char *buf = (char *)malloc(len+1);
	// SDL_RWread may return less bytes than requested, so we have to loop
	Sint64 tot_read = 0, cur_read = 0;
	char *pos = &buf[0];
	do {
		cur_read = SDL_RWread(fp, pos, 1, (len - tot_read));
		tot_read += cur_read;
		pos += cur_read;
	} while (tot_read < len && cur_read != 0);
	if (tot_read != len) {
		zerror("could not read file %s!", fname);
		return NULL;
	}
	buf[len] = '\0';
	SDL_RWclose(fp);
	return buf;
}
