#ifndef __MATH_H__
#define __MATH_H__

#define M_PI   3.14159265358979323846264338327
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.78539816339744830962
constexpr float M_PI_TIMES_2 = 2*M_PI;
constexpr float M_4_PI = (4 / M_PI);
constexpr float M_3_PI_2 = (3 * M_PI_2);
constexpr float M_DEG_TO_RAD (M_PI / 180.0f);

// Accurate to ~5 decimal places. Could change for more speed or precision.
float
__cos(float x)
{
	constexpr float c1 = 0.9999932946;
	constexpr float c2 = -0.4999124376;
	constexpr float c3 = 0.0414877472;
	constexpr float c4 = -0.0012712095;

	float x2 = x*x;
	return (c1 + x2*(c2 + x2*(c3 + c4 * x2)));
}

float
_cos(float x)
{
	// _cos() is only accurate from 0 to pi/2.

	assert(x < M_PI_TIMES_2); // We don't handle this yet...
	//x = fmod(x, M_PI_TIMES_2);
	//if (x < 0.0)
	//	x = -x;

	int quad = (int)(x / M_PI_2); // Which quadrant are we in?
	if (quad == 0)
		return __cos(x);
	else if (quad == 1)
		return -__cos(M_PI - x);
	else if (quad == 2)
		return -__cos(x - M_PI);
	else if (quad == 3)
		return __cos(M_PI_TIMES_2 - x);
	return 0.0;
}

float
_sin(float x)
{
	return _cos(M_PI_2 - x);
}

// Accurate to ~5 decimal places. Could change for more speed or precision.
float
__tan(float x)
{
	constexpr float c1 = -3.16783027;
	constexpr float c2 = 0.134516124;
	constexpr float c3 = -4.033321984;
	float x2 = x*x;
	return (x*(c1 + c2 * x2) / (c3 + x2));
}

float
_tan(float x)
{
	assert(x < M_PI_TIMES_2); // We don't handle this yet...
	//x = fmod(x, M_PI_TIMES_2);

	int octant = (int)(x / M_PI_4);

	// _tan() is only accurate from 0 to pi/4.
	// The approximation solves for tan((pi/2) * x), so we have to multiply by 4/pi.
	if (octant == 0)
		return __tan(x * M_4_PI);
	else if (octant == 1)
		return 1.0 / __tan((M_PI_2 - x) * M_4_PI);
	else if (octant == 2)
		return -1.0 / __tan((x - M_PI_2) * M_4_PI);
	else if (octant == 3)
		return -__tan((M_PI - x) * M_4_PI);
	else if (octant == 4)
		return __tan((x - M_PI) * M_4_PI);
	else if (octant == 5)
		return 1.0 / __tan((M_3_PI_2 - x) * M_4_PI);
	else if (octant == 6)
		return -1.0 / __tan((x - M_3_PI_2) * M_4_PI);
	else if (octant == 7)
		return -__tan((M_PI_TIMES_2 - x) * M_4_PI);
}

struct Vec2i {
	int x, y;
};

inline Vec2i
operator+(Vec2i a, Vec2i b)
{
	Vec2i result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

inline Vec2i
operator-(Vec2i a, Vec2i b)
{
	Vec2i result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

struct Vec2u {
	unsigned x, y;
};

struct Vec2f {
	float x, y;
	inline Vec2f &operator*=(float a);
	inline Vec2f &operator+=(float a);
	inline Vec2f &operator-=(float a);
};

inline Vec2f
operator*(float a, Vec2f b)
{
	Vec2f result;
	result.x = b.x * a;
	result.y = b.y * a;
	return result;
}

inline Vec2f
operator+(float a, Vec2f b)
{
	Vec2f result;
	result.x = b.x + a;
	result.y = b.y + a;
	return result;
}

inline Vec2f
operator+(Vec2f a, Vec2f b)
{
	Vec2f result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

inline Vec2f
operator-(float a, Vec2f b)
{
	Vec2f result;
	result.x = b.x - a;
	result.y = b.y - a;
	return result;
}

inline Vec2f
operator-(Vec2f a, Vec2f b)
{
	Vec2f result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

inline Vec2f &Vec2f::
operator*=(float a)
{
	*this = a * *this;
	return *this;
}

inline Vec2f &Vec2f::
operator+=(float a)
{
	*this = a + *this;
	return *this;
}

inline Vec2f &Vec2f::
operator-=(float a)
{
	*this = a - *this;
	return *this;
}

struct Vec3f {
	float x, y, z;
	inline Vec3f &operator+=(Vec3f v);
	inline Vec3f &operator-=(Vec3f v);
};

inline Vec3f
operator*(float a, Vec3f b)
{
	Vec3f result;
	result.x = b.x * a;
	result.y = b.y * a;
	result.z = b.z * a;
	return result;
}

inline Vec3f
operator-(Vec3f a, Vec3f b)
{
	Vec3f result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	return result;
}

inline Vec3f
operator+(Vec3f a, Vec3f b)
{
	Vec3f result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	return result;
}

inline Vec3f &Vec3f::
operator-=(Vec3f v)
{
	*this = v - *this;
	return *this;
}

inline Vec3f &Vec3f::
operator+=(Vec3f v)
{
	*this = v + *this;
	return *this;
}

struct Vec4f {
	float x, y, z, w;
};

// Quake approximation...
float
inv_sqrt( float number )
{
	long i;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;
	i  = 0x5f3759df - ( i >> 1 );
	y  = * ( float * ) &i;
	y  = y * ( threehalfs - ( x2 * y * y ) );

	return y;
}

inline float
length2(Vec3f v)
{
	return v.x*v.x + v.y*v.y + v.z*v.z;
}

inline Vec3f
normalize(Vec3f v)
{
	return inv_sqrt(length2(v)) * v;
}

inline float
dot_product(Vec3f a, Vec3f b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

inline Vec3f
cross_product(Vec3f a, Vec3f b)
{
	return { a.y*b.z - b.y*a.z,
	         a.z*b.x - b.z*a.x,
	         a.x*b.y - b.x*a.y };
}

/*
inline Vec3f
length(Vec3f v)
{

}
*/

struct Mat4 {
	float m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44;
	float &operator[](int i) { return (&m11)[i]; }
};

inline Mat4
perspective_matrix(float fovy, float aspect_ratio, float near, float far)
{
	float top = _tan((fovy / 2) * M_DEG_TO_RAD) * near;
	float right = top * aspect_ratio;
	return {
	           (near / right), 0, 0, 0,
	           0, (near / top), 0, 0,
	           0, 0, (-(far + near) / (far - near)), ((-2*far*near) / (far - near)),
	           0, 0, -1, 0
	       };
}

inline Mat4
ortho_matrix(float left, float right, float bottom, float top)
{
	return { 2.0f / (right - left), 0, 0, 0,
	         0, 2.0f / (top - bottom), 0, 0,
	         0, 0, -1.0f, 0,
	         -(right + left) / (right - left), -(top + bottom) / (top - bottom), 0, 0 };
}

inline Mat4
transpose(Mat4 m)
{
	return { m.m11, m.m21, m.m31, m.m41,
	         m.m12, m.m22, m.m32, m.m42,
	         m.m13, m.m23, m.m33, m.m43,
	         m.m14, m.m24, m.m34, m.m44 };
}

// Assumes z is a unit vector.
inline Mat4
view_matrix(Vec3f eye, Vec3f zaxis)
{
	//assert(length(front) <= 1.0f);
	zaxis = normalize(zaxis);
	Vec3f xaxis = normalize(cross_product(zaxis, { 0.0f, 1.0f, 0.0f }));
	Vec3f yaxis = cross_product(xaxis, zaxis);

	return { xaxis.x, yaxis.x, -zaxis.x, 0,
	         xaxis.y, yaxis.y, -zaxis.y, 0,
	         xaxis.z, yaxis.z, -zaxis.z, 0,
	         -dot_product(xaxis, eye), -dot_product(yaxis, eye), dot_product(zaxis, eye), 1 };
}

inline Mat4
operator*(float a, Mat4 m)
{
	Mat4 result;
	for (int i = 0; i < 16; ++i)
		result[i] = a * m[i];
	return result;
}

inline Mat4
inverse(const Mat4 &m)
{
	Mat4 inv;
	inv.m11 = m.m22 * m.m33 * m.m44 +
	          m.m23 * m.m34 * m.m42 +
	          m.m24 * m.m32 * m.m43 -
	          m.m22 * m.m34 * m.m43 -
	          m.m23 * m.m32 * m.m44 -
	          m.m24 * m.m33 * m.m42;

	inv.m12 = m.m12 * m.m34 * m.m43 +
	          m.m13 * m.m32 * m.m44 +
	          m.m14 * m.m33 * m.m42 -
	          m.m12 * m.m33 * m.m44 -
	          m.m13 * m.m34 * m.m42 -
	          m.m14 * m.m32 * m.m43;

	inv.m13 = m.m12 * m.m23 * m.m44 +
	          m.m13 * m.m24 * m.m42 +
	          m.m14 * m.m22 * m.m43 -
	          m.m12 * m.m24 * m.m43 -
	          m.m13 * m.m22 * m.m44 -
	          m.m14 * m.m23 * m.m42;

	inv.m14 = m.m12 * m.m24 * m.m33 +
	          m.m13 * m.m22 * m.m34 +
	          m.m14 * m.m23 * m.m32 -
	          m.m12 * m.m23 * m.m34 -
	          m.m13 * m.m24 * m.m32 -
	          m.m14 * m.m22 * m.m33;

	inv.m21 = m.m21 * m.m34 * m.m43 +
	          m.m23 * m.m31 * m.m44 +
	          m.m24 * m.m33 * m.m41 -
	          m.m21 * m.m33 * m.m44 -
	          m.m23 * m.m34 * m.m41 -
	          m.m24 * m.m31 * m.m43;

	inv.m22 = m.m11 * m.m33 * m.m44 +
	          m.m13 * m.m34 * m.m41 +
	          m.m14 * m.m31 * m.m43 -
	          m.m11 * m.m34 * m.m43 -
	          m.m13 * m.m31 * m.m44 -
	          m.m14 * m.m33 * m.m41;

	inv.m23 = m.m11 * m.m24 * m.m43 +
	          m.m13 * m.m21 * m.m44 +
	          m.m14 * m.m23 * m.m41 -
	          m.m11 * m.m23 * m.m44 -
	          m.m13 * m.m24 * m.m41 -
	          m.m14 * m.m21 * m.m43;

	inv.m24 = m.m11 * m.m23 * m.m34 +
	          m.m13 * m.m24 * m.m31 +
	          m.m14 * m.m21 * m.m33 -
	          m.m11 * m.m24 * m.m33 -
	          m.m13 * m.m21 * m.m34 -
	          m.m14 * m.m23 * m.m31;

	inv.m31 = m.m21 * m.m32 * m.m44 +
	          m.m22 * m.m34 * m.m41 +
	          m.m24 * m.m31 * m.m42 -
	          m.m21 * m.m34 * m.m42 -
	          m.m22 * m.m31 * m.m44 -
	          m.m24 * m.m32 * m.m41;

	inv.m32 = m.m11 * m.m34 * m.m42 +
	          m.m12 * m.m31 * m.m44 +
	          m.m14 * m.m32 * m.m41 -
	          m.m11 * m.m32 * m.m44 -
	          m.m12 * m.m34 * m.m41 -
	          m.m14 * m.m31 * m.m41;

	inv.m33 = m.m11 * m.m22 * m.m44 +
	          m.m12 * m.m24 * m.m41 +
	          m.m14 * m.m21 * m.m42 -
	          m.m11 * m.m24 * m.m42 -
	          m.m12 * m.m21 * m.m44 -
	          m.m14 * m.m22 * m.m41;

	inv.m34 = m.m11 * m.m24 * m.m32 +
	          m.m12 * m.m21 * m.m34 +
	          m.m14 * m.m22 * m.m31 -
	          m.m11 * m.m22 * m.m34 -
	          m.m12 * m.m24 * m.m31 -
	          m.m14 * m.m21 * m.m32;

	inv.m41 = m.m21 * m.m33 * m.m42 +
	          m.m22 * m.m31 * m.m43 +
	          m.m23 * m.m32 * m.m41 -
	          m.m21 * m.m32 * m.m43 -
	          m.m22 * m.m33 * m.m41 -
	          m.m23 * m.m31 * m.m42;

	inv.m42 = m.m11 * m.m32 * m.m43 +
	          m.m12 * m.m33 * m.m41 +
	          m.m13 * m.m31 * m.m42 -
	          m.m11 * m.m33 * m.m42 -
	          m.m12 * m.m31 * m.m43 -
	          m.m13 * m.m32 * m.m41;

	inv.m43 = m.m11 * m.m23 * m.m42 +
	          m.m12 * m.m21 * m.m43 +
	          m.m13 * m.m22 * m.m41 -
	          m.m11 * m.m22 * m.m43 -
	          m.m12 * m.m23 * m.m41 -
	          m.m13 * m.m21 * m.m42;

	inv.m44 = m.m11 * m.m22 * m.m33 +
                  m.m12 * m.m23 * m.m31 +
	          m.m13 * m.m21 * m.m32 -
	          m.m11 * m.m23 * m.m32 -
	          m.m12 * m.m21 * m.m33 -
	          m.m13 * m.m22 * m.m31;

	float det = m.m11 * inv.m11 + m.m12 * inv.m21 + m.m13 * inv.m31 + m.m14 * inv.m41;
	if (det == 0)
		return {}; // TEMP
	return (1.0f / det) * inv;
}

#endif
