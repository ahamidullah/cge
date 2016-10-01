#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include "zlib.h"

constexpr unsigned int max_err_len = 1024;

void
z_error(const char *filename, int linenum, const char *funcname, bool abort, const char *err_fmt, ...)
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

const char *
z_load_file(const char *fname)
{
	SDL_RWops *fp = SDL_RWFromFile(fname, "r");
	if (!fp) {
		z_printerr("error loading shader file %s!\n", fname);
		return NULL;
	}
	Sint64 file_len = SDL_RWsize(fp);
	if (file_len < 0) {
		z_printerr("could not get %s file length! SDL Error: %s\n", fname, SDL_GetError());
		return NULL;
	}
	char *buf = new char [file_len+1];
	// SDL_RWread may return less bytes than requested, so we have to loop
	Sint64 total_bytes_read = 0, cur_bytes_read = 0;
	char *cur_buf_loc = buf;
	do {
		cur_bytes_read = SDL_RWread(fp, cur_buf_loc, 1, (file_len - total_bytes_read));
		total_bytes_read += cur_bytes_read;
		cur_buf_loc += cur_bytes_read;
	} while (total_bytes_read < file_len && cur_bytes_read != 0);

	if (total_bytes_read != file_len) {
		delete [] buf;
		z_printerr("error read reading file %s!", fname);
		return NULL;
	}
	buf[file_len] = '\0';
	SDL_RWclose(fp);
	return buf;
}

// free list array
template <typename T>
Z_Array<T>::Z_Array(const uint16_t cap)
{
	capacity = cap;
	size = 0;
	//items = z_malloc<Item<T>>(cap);
	next_key = 1;
	free_head = -1;
}

template <typename T>
Item<T> *
Z_Array<T>::alloc()
{
	assert(size >= 0 && size <= capacity);
	// TODO: should we memset/construct allocd items? slower, but less bug prone
	if (free_head >= 0) { // use a freed element
		assert(free_head <= 0x0000FFFF); // free head is past the end of the array
		Item<T> *new_elem = &items[free_head];
		int32_t next_free = new_elem->id;
		new_elem->id = (next_key++ << 16 | free_head);
		free_head = next_free;
		return new_elem;
	}

	// get a new one off the end
	if (size == capacity) {
		uint16_t new_capacity = capacity * 2;
		assert(new_capacity == capacity * 2); // capacity has wrapped around
		items = z_realloc<Item<T>>(items, new_capacity);
		capacity = new_capacity;
	}

	Item<T> *new_elem = &items[size];
	new_elem->id = (next_key++ << 16 | size);
	++size;
	return new_elem;
}

#define GET_IND(id) (id & 0x0000FFFF)

template<typename T>
Item<T> *
Z_Array<T>::get(const int32_t id)
{
	printf("%d\n", GET_IND(id));
	return &items[GET_IND(id)];
}

template<typename T>
void
Z_Array<T>::free(const int32_t id)
{
	items[GET_IND(id)].id = free_head;
	free_head = GET_IND(id);
}

#undef GET_IND

// bit vector -- initialized to zero
Z_Bit_Vector::Z_Bit_Vector(size_t num_bits)
{
	int remainder = (num_bits%8) ? 1 : 0;
	bits = z_calloc<uint8_t>((num_bits/8) + remainder);
}

Z_Bit_Vector::~Z_Bit_Vector()
{
	z_free(bits);
}

void
Z_Bit_Vector::set(size_t i)
{
	size_t char_ofs = i/8;
	size_t bit_ofs = i%8;
	bits[char_ofs] |= (1 << bit_ofs);
}

void
Z_Bit_Vector::clear(size_t i)
{
	size_t char_ofs = i/8;
	size_t bit_ofs = i%8;
	bits[char_ofs] &= ~(1 << bit_ofs);
}

bool
Z_Bit_Vector::get(size_t i)
{
	size_t char_ofs = i/8;
	size_t bit_ofs = i%8;
	return bits[char_ofs] & (1 << bit_ofs); // c++ does the bool cast for us
}

template class Z_Array<int>;
