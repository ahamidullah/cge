#ifndef __ZLIB_H__
#define __ZLIB_H__

#include <string>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <type_traits>
#include <stdlib.h>
#include <experimental/optional>

namespace std {
	using std::experimental::optional;
}

void error(const char *, int, const char *, bool, const char *, ...);
#define zabort(fmt, ...) error(__FILE__, __LINE__, __func__, true, fmt, ## __VA_ARGS__)
#define zerror(fmt, ...) error(__FILE__, __LINE__, __func__, false, fmt, ## __VA_ARGS__)
#define ARR_LEN(x) (sizeof(x)/sizeof(x[0]))

std::optional<std::string> load_file(const char *, const char *);

struct vec3f {
	float x;
	float y;
	float z;
};

#endif
