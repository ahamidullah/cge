#ifndef __ZLIB_H__
#define __ZLIB_H__

typedef unsigned int u32;
typedef int s32;

#define ABORT(fmt, ...) zerror(__FILE__, __LINE__, __func__, true, fmt, ## __VA_ARGS__)
#define PRINTERR(fmt, ...) zerror(__FILE__, __LINE__, __func__, false, fmt, ## __VA_ARGS__)
#define ARR_LEN(x) (sizeof(x)/sizeof(x[0]))

namespace zlib {

void zerror(const char *filename, int linenum, const char *funcname, bool abort, const char *err_fmt, ...);
void *zmalloc(size_t sz);	
void zfree(void* p);

} //end zlib

#endif