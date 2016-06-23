#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <assert.h>
#include "sys.h"

#define MAX_ERR_LEN 1024

namespace sys {

void
zerror(const char *filename, int linenum, const char *funcname, bool abort, const char *err_fmt, ...)
{
	va_list argptr;
	char err_msg[MAX_ERR_LEN];
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

}

} //end sys
