constexpr unsigned
round_nearest(float f)
{
	return (unsigned)(f + 0.5f);
}

constexpr unsigned
round_up(float f)
{
	return (unsigned)(f + 1.0f);
}

template <typename F>
struct Scope_Exit {
	Scope_Exit(F _f) : f(_f) {}
	~Scope_Exit() { f(); }
	F f;
};

template <typename F>
Scope_Exit<F>
make_scope_exit(F f)
{
	return Scope_Exit<F>(f);
}

#define DEFER(code) auto scope_exit_##__LINE__ = make_scope_exit([=](){code;})

void
strcpy(char *dest, char *src)
{
	while(*src)
		*dest++ = *src++;
}

void
strrev(char *start, char *end)
{
	while(start < end) {
		char tmp = *start;
		*start++ = *end;
		*end-- = tmp;
	}
}

size_t
push_integer(int i, char *buf)
{
	size_t nbytes_writ = 0;
	if (i < 0) {
		buf[nbytes_writ++] = '-';
		i = -i;
	}
	char *start = buf + nbytes_writ;
	do {
		char ascii = (i % 10) + 48;
		buf[nbytes_writ++] = ascii;
		i /= 10;
	} while (i > 0);
	char *end = buf + nbytes_writ;
	strrev(start, end);
	return nbytes_writ;
}

// TODO: Handle floating point!!!!!
size_t
format_string(const char *fmt, va_list arg_list, char *buf)
{
	size_t nbytes_writ = 0;
	for (const char *at = fmt; *at;) {
		if (*at == '%') {
			++at;
			switch (*at) {
			case 's': {
				char *s = va_arg(arg_list, char *);
				while (*s) {
					buf[nbytes_writ++] = *s++;
				}
				break;
			}
			case 'd': {
				int i = va_arg(arg_list, int);
				nbytes_writ += push_integer(i, buf + nbytes_writ);
				break;
			}
			case 'c': {
				buf[nbytes_writ++] = va_arg(arg_list, int);
				break;
			}
			case 'l': {
				long l = va_arg(arg_list, long);
				nbytes_writ += push_integer(l, buf + nbytes_writ);
				break;
			}
			}
			++at;
		}
		buf[nbytes_writ++] = *at++;
	}
	return nbytes_writ;
}

void
debug_print(const char *fmt, va_list args)
{
	char buf[4096];
	size_t nbytes_writ = format_string(fmt, args, buf);
	platform_debug_print(nbytes_writ, buf);
}

void
debug_print(const char *fmt, ...)
{
	char buf[4096];
	va_list arg_list;
	va_start(arg_list, fmt);
	size_t nbytes_writ = format_string(fmt, arg_list, buf);
	platform_debug_print(nbytes_writ, buf);
	va_end(arg_list);
}

void
error(const char *file, int line, const char *func, bool abort, const char *fmt, ...)
{
	debug_print("Error: %s:%d in %s - ", file, line, func);
	va_list args;
	va_start(args, fmt);
	debug_print(fmt, args);
	va_end(args);
	debug_print("\n");
	if (abort)
		exit(1);
}

#define TIMED_BLOCK(name) Block_Timer __block_timer__##__LINE__(#name)
struct Block_Timer {
	Block_Timer(const char *n)
	{
		strcpy(name, n); 
		start = platform_get_time();
	}
	~Block_Timer()
	{
		Platform_Time end = platform_get_time();
		unsigned ns_res=1, mus_res=1000, ms_res=1000000;
		long ns = platform_time_diff(start, end, ns_res);
		long ms = platform_time_diff(start, end, ms_res);
		long mus = platform_time_diff(start, end, mus_res);
		debug_print("%s - %dns %dmus %dms\n", name, ns, mus, ms);
	}
	Platform_Time start;
	char name[256];
};

bool
get_base_name(const char *path, char *name_buf)
{
	int begin = 0;
	while(path[begin] && path[begin] != '/')
		++begin;
	if (!path[begin])
		return false;
	int end = ++begin;
	while(path[end] && path[end] != '.')
		++end;
	strncpy(name_buf, path+begin, end-begin);
	return true;
}

// Array
template <typename T>
struct Array {
	size_t size;
	size_t capacity;
	T *elems;
	T &operator[](size_t i) { assert(i < size); return elems[i]; }
};

template <typename T>
//Array<T>
void
make_array(size_t init_capacity = 0)
{
	//a->size = 0;
	//a->capacity = init_capacity;
	//a->elems = (T *)malloc(sizeof(T)*init_capacity);
}

template <typename T>
void
array_clear(Array<T> *a)
{
	a->size = 0;
}

template <typename T>
void
array_destroy(Array<T> *a)
{

}

template <typename T>
T *
array_alloc(Array<T> *a, size_t count = 1)
{
	if (a->size + count > a->capacity) {
		a->capacity = (a->capacity + count) * 2;
		// TODO: Do realloc properly.
		a->elems = (T *)realloc(a->elems, sizeof(T) * a->capacity);
	}
	T *start = &a->elems[a->size];
	a->size += count;
	return start;
}

template <typename T>
T *
begin(Array<T> &a)
{
	return &a.elems[0];
}

template <typename T>
T *
end(Array<T> &a)
{
	return &a.elems[a.size];
}

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
	Array<Element<T>> elems;
};

template <typename T>
void
pool_init(Pool<T> *p)
{
	p->key = 1;
	p->free_head = -1;
	//p->last_allocd = -1;
	array_init(&p->elems);
}

template <typename T>
T *
pool_alloc(Pool<T> *p)
{
	int index;
	if (p->free_head != -1) {
		index = p->free_head;
		//new_elem = &p->elems[p->free_head];
		p->free_head = p->elems[p->free_head].id & INDEX_MASK;
	} else {
	/*
		if (++p->last_allocd >= p->capacity) {
			p->capacity *= 2;
			assert(p->capacity <= POOL_MAX);
			p->elems = (Element<T> *)realloc(p->elems, sizeof(Element<T>) * p->capacity);
		}
	*/
		index = p->elems.size;
		array_alloc(p->elems);
	}
	//p->elems[index].id = (p->key++ << NUM_INDEX_BITS) | index;
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
	for (const auto &e : p.elems) {
		if ((e.id & KEY_MASK) >> NUM_INDEX_BITS)
			return &e.data;
	}
	return NULL;
}

template <typename T>
void
pool_destroy(Pool<T> *p)
{
	array_destroy(p->elems);
}

template <typename T>
T *
pool_next(const Pool<T> &p, T *e)
{
	int e_ind = pool_getid(e) & INDEX_MASK;
	for (int i = e_ind+1; i < p.elems.size; ++i) {
		if ((p.elems[i].id & KEY_MASK) >> NUM_INDEX_BITS)
			return &p.elems[i].data;
	}
	return NULL;
}

// Flat Map
template <typename K, typename V>
struct Map_Element {
	K key;
	V value;
};

template <typename K, typename V, typename F>
struct Map {
	Pool<Map_Element<K, V>> melems;
	F hash;
};

template <typename K, typename V, typename F>
void
map_init(Map<K, V, F> m)
{
	
}
