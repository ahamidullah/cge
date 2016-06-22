#ifndef __SYS_H__
#define __SYS_H__

#include <vector>

namespace sys {

void print_err(char *err_fmt, ...);
void cge_abort(char *err_fmt, ...);
void *cge_malloc(size_t size);
void cge_free(void *p);
std::string load_file(const char *filename);

} //end sys
#endif