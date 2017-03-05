#ifndef __LIB_H__
#define __LIB_H__

struct Vec2s {
	size_t x;
	size_t y;
};

struct Vec2u {
	unsigned x;
	unsigned y;
};

struct Vec2i {
	int x;
	int y;
};

struct Vec2f {
	float x;
	float y;
};

struct Vec3f {
	float x;
	float y;
	float z;
};

#define zabort(fmt, ...) error(__FILE__, __LINE__, __func__, true, fmt, ## __VA_ARGS__)
#define zerror(fmt, ...) error(__FILE__, __LINE__, __func__, false, fmt, ## __VA_ARGS__)

#define ARR_LEN(a) (sizeof(a)/sizeof(a[0]))

#define KILOBYTE(b) ((size_t)b*1024)
#define MEGABYTE(b) (KILOBYTE(b)*1024)
#define GIGABYTE(b) (MEGABYTE(b)*1024)

#endif
