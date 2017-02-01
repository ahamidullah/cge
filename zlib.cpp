#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <dirent.h>
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

int
load_files(const char (*fnames)[256], const char *mode, int n, char **fdata)
{
	int succs = 0;
	for (int i = 0; i < n; ++i) {
		SDL_RWops *fp = SDL_RWFromFile(fnames[i], mode);
		if (!fp) {
			zerror("could not load file %s!", fnames[i]);
			fdata[i] = NULL;
			continue;
		}
		Sint64 len = SDL_RWsize(fp);
		if (len < 0) {
			zerror("could not get %s file length! SDL Error: %s\n", fnames[i], SDL_GetError());
			fdata[i] = NULL;
			continue;
		}
		fdata[succs] = (char *)malloc(len+1);
		// SDL_RWread may return less bytes than requested, so we have to loop
		Sint64 tot_read = 0, cur_read = 0;
		char *pos = &fdata[succs][0];
		do {
			cur_read = SDL_RWread(fp, pos, 1, (len - tot_read));
			tot_read += cur_read;
			pos += cur_read;
		} while (tot_read < len && cur_read);
		if (tot_read != len) {
			zerror("could not read file %s!", fnames[i]);
			free(fdata[succs]);
			continue;
		}
		fdata[succs][len] = '\0';
		SDL_RWclose(fp);
		succs++;
	}
	return succs;
}

void
free_files(char **fdata, unsigned num)
{
	for (int i = 0; i < num; ++i) {
		free(fdata[i]);
	}
}

int
get_fnames(const char *dir_name, char (*fnames)[256])
{
	DIR *dir;
	struct dirent *ent;
	int num_files = 0;

	dir = opendir(dir_name);
	if (dir) {
		while ((ent = readdir(dir)) != NULL) {
			if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
				continue;
			strcpy(fnames[num_files], dir_name);
			strcat(fnames[num_files], ent->d_name);
			++num_files;
		}
		closedir(dir);
	} else
		zerror("could not open directory %s", dir_name);
	return num_files;
}

