#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <vector>

#define MAX_ERR_LEN 1024

namespace sys {

void
print_err(const char *err_fmt, ...)
{
	va_list argptr;
	char err_msg[MAX_ERR_LEN];
	va_start(argptr, err_fmt);
	vsprintf(err_msg, err_fmt, argptr);
	va_end(argptr);
}

void
cge_abort(const char *err_fmt, ...)
{
	va_list argptr;
	char err_msg[MAX_ERR_LEN];
	va_start(argptr, err_fmt);
	vsprintf(err_msg, err_fmt, argptr);
	va_end(argptr);
	exit(1);
}

void *
cge_malloc(size_t size)
{
	void *n = malloc(size);
	if (!n)
		cge_abort("malloc failed\n");
	return n;
}

std::string
load_file(char *buf, const char *filename)
{
	FILE *f = fopen(filename, "r");
	if (!f) {
		print_err("could not find file %s!", filename);
		return NULL;
	}
	struct stat filestat;
	if (stat(filename, &filestat) < 0) {
		print_err("could not fstat file %s!", filename);
		return NULL;
	}
	std::vector<char> buf(filestat.st_size+1);
	const char *mytry = buf;
	return buf;
}

void
cge_free(void* p)
{

}

} //end sys
