#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include "zlib.h"

constexpr unsigned int max_err_len = 1024;

void
error(const char *filename, int linenum, const char *funcname, bool abort, const char *err_fmt, ...)
{
	va_list argptr;
	char err_msg[max_err_len];
	va_start(argptr, err_fmt);
	vsprintf(err_msg, err_fmt, argptr);
	va_end(argptr);
	fprintf(stderr, "Error: %s:%d in %s - %s\n", filename, linenum, funcname, err_msg);
	if (abort)
		exit(1);
}

std::optional<std::string>
load_file(const char *fname, const char *mode)
{
	SDL_RWops *fp = SDL_RWFromFile(fname, mode);
	if (!fp) {
		z_log_err("error loading file %s!\n", fname);
		return {};
	}
	Sint64 file_len = SDL_RWsize(fp);
	if (file_len < 0) {
		z_log_err("could not get %s file length! SDL Error: %s\n", fname, SDL_GetError());
		return {};
	}
	std::string buf;
	buf.resize(file_len+1);
	// SDL_RWread may return less bytes than requested, so we have to loop
	Sint64 total_bytes_read = 0, cur_bytes_read = 0;
	char *cur_buf_loc = &buf[0];
	do {
		cur_bytes_read = SDL_RWread(fp, cur_buf_loc, 1, (file_len - total_bytes_read));
		total_bytes_read += cur_bytes_read;
		cur_buf_loc += cur_bytes_read;
	} while (total_bytes_read < file_len && cur_bytes_read != 0);

	if (total_bytes_read != file_len) {
		z_log_err("error read reading file %s!", fname);
		return {};
	}
	buf[file_len] = '\0';
	SDL_RWclose(fp);
	return buf;
}
