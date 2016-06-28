#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <assert.h>
#include "zlib.h"

enum {
	max_err_len = 1024,
};

namespace zlib {

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

void *
zmalloc(size_t sz)
{
	assert(sz > 0);
	void *n = malloc(sz);
	if (!n)
		ABORT("malloc failed\n");
	return n;
}
	
void
zfree(void* p)
{
	free(p);
}

std::string
load_file(const char *fname)
{
	FILE *fp = fopen(fname, "r");
	if (!fp)
		zlib::ABORT("error loading shader file %s!\n", fname);
	if (fseek(fp, 0L, SEEK_END) != 0)
		zlib::ABORT("could not fseek shader file %s!\n", fname);
	long filelen = ftell(fp);
	if (filelen == -1)
		zlib::ABORT("could not ftell shader file %s!\n", fname);
	std::string buf;
	buf.resize(filelen);
	if (fseek(fp, 0L, SEEK_SET) != 0) //SEEK_SET sets fp to beginning
		zlib::ABORT("could not fseek shader file %s!\n", fname);
	size_t fileend = fread(&buf[0], sizeof(char), filelen, fp);
	if (ferror(fp))
		zlib::ABORT("error read reading file %s!", fname);
	buf[fileend] = '\0';
	fclose(fp);
	return buf;
}

} //end zlib
