#ifndef __ZLIB_H__
#define __ZLIB_H__

#include <string>
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <experimental/optional>

namespace std {
	using std::experimental::optional;
}

#define MAX_PATH_LEN 256

// pool -- array of objects that allows deleted objects to be reused rather than
// shifting elements around. Each object has a unique ID.

#define POOL_MAX 0xFFFF
#define INDEX_MASK 0x0000FFFF
#define KEY_MASK 0xFFFF0000
#define NUM_INDEX_BITS 16

template <typename T>
struct Element {
	T data;
	uint32_t id; // for allocd: upper 16 bits are unique key, lower 16 are its index
	             // for freed:  upper 16 are 0, lower 16 are index of next freed
};

template <typename T>
struct Pool {
	uint32_t key;
	int free_head;
	int last_allocd; // element closest to the end that was ever allocd
	int capacity;
	Element<T> *elems;
};

template <typename T>
void
pool_init(Pool<T> *p)
{
	p->key = 1;
	p->free_head = -1;
	p->last_allocd = -1;
	p->capacity = 2;
	p->elems = (Element<T> *)malloc(sizeof(Element<T>)*p->capacity);
}

template <typename T>
T *
pool_alloc(Pool<T> *p)
{
	int index;
	if (p->free_head != -1) {
		index = p->free_head;
		p->free_head = p->elems[p->free_head].id & INDEX_MASK;
	} else {
		if (++p->last_allocd >= p->capacity) {
			p->capacity *= 2;
			assert(p->capacity <= POOL_MAX);
			p->elems = (Element<T> *)realloc(p->elems, sizeof(Element<T>) * p->capacity);
		}
		index = p->last_allocd;
	}
	p->elems[index].id = (p->key++ << NUM_INDEX_BITS) | index;
	return &p->elems[index].data;
}

template <typename T>
uint32_t
pool_getid(T *data)
{
	return *(uint32_t *)(((char *)data) + offsetof(Element<T>, id));
}

template <typename T>
T *
pool_get(const Pool<T> &p, int id)
{
	assert(p.elems[id & INDEX_MASK].id == id);
	return &p.elems[id & INDEX_MASK].data;
}

template <typename T>
T *
pool_first(const Pool<T> &p)
{
	for (int i = 0; i <= p.last_allocd; ++i) {
		if ((p.elems[i].id & KEY_MASK) >> NUM_INDEX_BITS)
			return &p.elems[i].data;
	}
	return NULL;
}

template <typename T>
void
pool_destroy(const Pool<T> &p)
{
	free(p.elems);
}

template <typename T>
T *
pool_next(const Pool<T> &p, T *e)
{
	int e_ind = pool_getid(e) & INDEX_MASK;
	for (int i = e_ind+1; i <= p.last_allocd; ++i) {
		if ((p.elems[i].id & KEY_MASK) >> 16)
			return &p.elems[i].data;
	}
	return NULL;
}

// hash table
template <typename V, typename F>
struct Hash_Table {
	F hash_fn;
	V data[100]; // temp
	bool valid[100];
};

template<typename V, typename F>
void
hsh_init(Hash_Table<V,F> *ht, F hash_fn)
{
	ht->hash_fn = hash_fn;
	for (int i = 0; i < 100; ++i)
		ht->valid[i] = false;
}

template <typename K, typename V, typename F>
void
hsh_add(Hash_Table<V,F> *ht, K key, V val)
{
	int ind = ht->hash_fn(key);
	ht->data[ind] = val;
	ht->valid[ind] = true;
}

template <typename K, typename V, typename F>
void
hsh_add_batch(Hash_Table<V,F> *ht, K *keys, V *vals, int n)
{
	for (int i = 0; i < n; ++i) {
		int ind = ht->hash_fn(keys[i]);
		ht->data[ind] = vals[i];
		ht->valid[ind] = true;
	}
}

template <typename K, typename V, typename F>
std::optional<V>
hsh_get(const Hash_Table<V,F> &ht, K key)
{
	int i = ht.hash_fn(key);
	if (!ht.valid[i])
		return {};
	return ht.data[i];
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
int load_files(const char (*)[256], const char *, int, char **);
void free_files(char **, unsigned);
int get_paths(const char *, char (*)[256]);

#endif
