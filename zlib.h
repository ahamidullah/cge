#ifndef __ZLIB_H__
#define __ZLIB_H__

#include <string>
#include <stddef.h>
#include <assert.h>
#include <new> // no throw

class Z_Bit_Vector {
public:
	Z_Bit_Vector(size_t); // specified in bits, max size_t bits
	~Z_Bit_Vector();
	void set(size_t);
	void clear(size_t);
	bool get(size_t);
private:
	uint8_t *bits;
};

struct vec3f {
	float x;
	float y;
	float z;
};

template <typename T>
struct Item {
	T data;
	int32_t id; // (next_key++ << 16 | index) for alloced entries, (0 | next_free_index) for free list entries
};

// max array size is ~65K (uint16 max)
template <typename T>
struct Z_Array {
	Z_Array(const uint16_t);
	Item<T> *alloc();
	void free(const int32_t);
	Item<T> *get(const int32_t);

	Item<T> *items;
	uint16_t capacity;
	uint16_t size; // freed elements still count towards size
	uint32_t next_key; // should never equal to 0, we only use the lower 2 bytes -- wraps around at ~65k allocs
	int32_t free_head; // index of most recently freed entry, -1 if no free entries
};

#define z_abort(fmt, ...) z_error(__FILE__, __LINE__, __func__, true, fmt, ## __VA_ARGS__)
#define z_printerr(fmt, ...) z_error(__FILE__, __LINE__, __func__, false, fmt, ## __VA_ARGS__)
#define z_arr_len(x) (sizeof(x)/sizeof(x[0]))

void z_error(const char *filename, int linenum, const char *funcname, bool abort, const char *err_fmt, ...);
const char *z_load_file(const char *fname);

// memory
template <typename T>
T *
z_malloc(size_t num = 1)
{
	assert(num >= 0);

	T *ptr = (T *)malloc(sizeof(T)*num);
	if (!ptr) // let's just abort for now if we fail to malloc
		z_abort("memory allocation failed!");
	return ptr;
}

template <typename T>
T *
z_realloc(T *ptr, size_t num)
{
	assert(num > 0);

	ptr = (T *)realloc(ptr, sizeof(T)*num);
	if (!ptr) // let's just abort for now if we fail to malloc
		z_abort("memory reallocation failed!");
	return ptr;
}

template <typename T>
T *
z_calloc(size_t num)
{
	assert(num > 0);

	T *ptr = (T *)calloc(num, sizeof(T));
	if (!ptr) // let's just abort for now if we fail to malloc
		z_abort("memory callocation failed!");
	return ptr;
}

template <typename T>
void
z_free(T *ptr)
{
	assert(ptr);

	free(ptr);
}

#endif
