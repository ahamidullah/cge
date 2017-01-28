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

// array
#define ARRAY_MAX 0xFFFF
#define INDEX_MASK 0x0000FFFF
#define KEY_MASK   0xFFFF0000

template <typename T>
struct Element {
	T data;
	int id; // for allocd: upper 16 bits are uniqueish key, lower 16 are index into elems
		        // for freed:  upper 16 are 0, lower 16 are index of next freed
};

template <typename T>
struct Array {
	int key;
	int free_head;
	int last_allocd;
	int len;
	Element<T> *elems;
};

template <typename T>
void
arr_init(Array<T> *a)
{
	a->key = 1;
	a->free_head = -1;
	a->last_allocd = -1;
	a->len = 2;
	a->elems = (Element<T> *)malloc(sizeof(Element<T>) * a->len);
}

template <typename T>
T *
arr_alloc(Array<T> *a)
{
	int index;
	if (a->free_head != -1) {
		index = a->free_head;
		a->free_head = a->elems[a->free_head].id & INDEX_MASK;
	} else {
		if (++a->last_allocd >= a->len) {
			a->len *= 2;
			assert(a->len <= ARRAY_MAX);
			a->elems = (Element<T> *)realloc(a->elems, sizeof(Element<T>) * a->len);
		}
		index = a->last_allocd;
	}
	a->elems[index].id = a->key++ << 16 | index;
	return &a->elems[index].data;
}

template <typename T>
int
arr_getid(T *data)
{
	return *(int *)(((char *)data) + offsetof(Element<T>, id));
}

template <typename T>
T *
arr_get(const Array<T> &a, int id)
{
	assert(a.elems[id & INDEX_MASK].id == id);
	return &a.elems[id & INDEX_MASK].data;
}

template <typename T>
T *
arr_first(const Array<T> &a)
{
	for (int i = 0; i <= a.last_allocd; ++i) {
		if ((a.elems[i].id & KEY_MASK) >> 16)
			return &a.elems[i].data;
	}
	return NULL;
}

template <typename T>
void
arr_destroy(const Array<T> &a)
{
	free(a.elems);
}

template <typename T>
T *
arr_next(const Array<T> &a, T *e)
{
	int e_ind = arr_getid(e) & INDEX_MASK;
	for (int i = e_ind+1; i <= a.last_allocd; ++i) {
		if ((a.elems[i].id & KEY_MASK) >> 16)
			return &a.elems[i].data;
	}
	return NULL;
}

// hash table
template <typename V, typename F>
struct Hash_Table {
	F hash_fn;
	V data[100];
};

template<typename V, typename F>
void
hsh_init(Hash_Table<V,F> *ht, F hash_fn)
{
	ht->hash_fn = hash_fn;
}

template <typename K, typename V, typename F>
void
hsh_add(Hash_Table<V,F> *ht, K key, V val)
{
	ht->data[ht->hash_fn(key)] = val;
}

template <typename K, typename V, typename F>
V
hsh_get(const Hash_Table<V,F> &ht, K key)
{
	return ht.data[ht.hash_fn(key)];
}

template<typename V, typename F>
void
hsh_destroy(Hash_Table<V,F> *ht)
{
	return; // nothing for now...
}

void error(const char *, int, const char *, bool, const char *, ...);
#define zabort(fmt, ...) error(__FILE__, __LINE__, __func__, true, fmt, ## __VA_ARGS__)
#define zerror(fmt, ...) error(__FILE__, __LINE__, __func__, false, fmt, ## __VA_ARGS__)
#define ARR_LEN(x) (sizeof(x)/sizeof(x[0]))

char *load_file(const char *, const char *);

#endif
