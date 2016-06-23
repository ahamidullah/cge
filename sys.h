#ifndef __SYS_H__
#define __SYS_H__

#define ABORT(fmt, ...) sys::zerror(__FILE__, __LINE__, __func__, true, fmt, ## __VA_ARGS__)
#define PRINTERR(fmt, ...) sys::zerror(__FILE__, __LINE__, __func__, false, fmt, ## __VA_ARGS__)

namespace sys {

void zerror(const char *filename, int linenum, const char *funcname, bool abort, const char *err_fmt, ...);
void *zmalloc(size_t sz);
void zfree(void *p);

} //end sys
#endif