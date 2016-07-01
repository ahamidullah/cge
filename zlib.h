#ifndef __ZLIB_H__
#define __ZLIB_H__

#include <string>
#include <stddef.h>

#define internal static

typedef uint32_t u32;
typedef int32_t s32;
typedef uint8_t u8;

#define ABORT(fmt, ...) error(__FILE__, __LINE__, __func__, true, fmt, ## __VA_ARGS__)
#define PRINTERR(fmt, ...) error(__FILE__, __LINE__, __func__, false, fmt, ## __VA_ARGS__)
#define ARR_LEN(x) (sizeof(x)/sizeof(x[0]))

namespace zlib {

void error(const char *filename, int linenum, const char *funcname, bool abort, const char *err_fmt, ...);
void *zmalloc(size_t sz);
void zfree(void* p);
std::string load_file(const char *fname);

}

#endif