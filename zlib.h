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

#define abort(fmt, ...) error(__FILE__, __LINE__, __func__, true, fmt, ## __VA_ARGS__)
#define log_error(fmt, ...) error(__FILE__, __LINE__, __func__, false, fmt, ## __VA_ARGS__)
#define ARR_LEN(x) (sizeof(x)/sizeof(x[0]))

//void error(const char *, int, const char *, bool, const char *, ...);
std::optional<std::string> load_file(const char *, const char *);

struct vec3f {
	float x;
	float y;
	float z;
};

#endif
