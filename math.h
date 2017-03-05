#ifndef __MATH_H__
#define __MATH_H__

struct V2i {
	int x, y;
};

inline V2i
operator+(V2i a, V2i b)
{
	V2i result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

inline V2i
operator-(V2i a, V2i b)
{
	V2i result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

struct V2u {
	unsigned x, y;
};

struct V2f {
	float x, y;
	inline V2f &operator*=(float a);
	inline V2f &operator+=(float a);
	inline V2f &operator-=(float a);
};

inline V2f
operator*(float a, V2f b)
{
	V2f result;
	result.x = b.x * a;
	result.y = b.y * a;
	return result;
}

inline V2f
operator+(float a, V2f b)
{
	V2f result;
	result.x = b.x + a;
	result.y = b.y + a;
	return result;
}

inline V2f
operator+(V2f a, V2f b)
{
	V2f result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

inline V2f
operator-(float a, V2f b)
{
	V2f result;
	result.x = b.x - a;
	result.y = b.y - a;
	return result;
}

inline V2f
operator-(V2f a, V2f b)
{
	V2f result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

inline V2f &V2f::
operator*=(float a)
{
	*this = a * *this;
	return *this;
}

inline V2f &V2f::
operator+=(float a)
{
	*this = a + *this;
	return *this;
}

inline V2f &V2f::
operator-=(float a)
{
	*this = a - *this;
	return *this;
}

struct V3f {
	float x, y, z;
};

#endif
