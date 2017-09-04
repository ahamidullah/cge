#ifndef __MATH_H__
#define __MATH_H__

#define M_PI   3.14159265358979323846264338327
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.78539816339744830962
constexpr float M_PI_TIMES_2 = 2*M_PI;
constexpr float M_4_PI = (4 / M_PI);
constexpr float M_3_PI_2 = (3 * M_PI_2);
constexpr float M_DEG_TO_RAD (M_PI / 180.0f);

constexpr int round_nearest(float f);

// Not sure if this is great...
float
_fmod(float x, float y)
{
	return x - (round_nearest(x/y)*y);
}

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
_cos(float x2)
{
	// _cos() is only accurate from 0 to pi/2.

	float x = _fmod(x2, M_PI_TIMES_2);
	if (x < 0.0f)
		x = -x;

	int quad = (int)(x / M_PI_2); // Which quadrant are we in?
	if (quad == 0)
		return __cos(x);
	else if (quad == 1)
		return -__cos(M_PI - x);
	else if (quad == 2)
		return -__cos(x - M_PI);
	else if (quad == 3)
		return __cos(M_PI_TIMES_2 - x);
	assert(0);
	return 0.0f;
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
	assert(0);
	return 0.0f;
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
operator/(Vec3f v, float divisor)
{
	Vec3f res;
	res.x = v.x / divisor;
	res.y = v.y / divisor;
	res.z = v.z / divisor;
	return res;
}

inline Vec3f
operator-(Vec3f v)
{
	return -1.0f * v;
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
	*this = *this - v;
	return *this;
}

inline Vec3f &Vec3f::
operator+=(Vec3f v)
{
	*this = *this + v;
	return *this;
}

struct Vec4f {
	float x, y, z, w;
	inline Vec4f &operator/=(float v);
};

inline Vec4f
operator/(Vec4f v, float s)
{
	Vec4f result;
	result.x = v.x / s;
	result.y = v.y / s;
	result.z = v.z / s;
	result.w = v.w / s;
	return result;
}

inline Vec4f &Vec4f::
operator/=(float v)
{
	*this = *this / v;
	return *this;
}

// Quake approximation...
float
inv_sqrt(float number)
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
dot_product(Vec4f a, float *b)
{
	return a.x*b[0] + a.y*b[1] + a.z*b[2] + a.w*b[3];
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
	//float m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44;
	//float m11, m21, m31, m41, m12, m22, m32, m42, m13, m23, m33, m43, m14, m24, m34, m44;
	float m[16];
	float &operator[](int i) { return m[i]; }
	float &operator()(int row, int col) { return m[((col - 1) << 2) + (row - 1)]; }
};

inline Mat4
perspective_matrix(float fovy, float aspect_ratio, float near, float far)
{
	//float top = _tan((fovy / 2) * M_DEG_TO_RAD);
	float top = _tan((fovy / 2) * M_DEG_TO_RAD) * near;
	float right = top * aspect_ratio;
	return { (near / right), 0, 0, 0,
	         0, (near / top), 0, 0,
	         0, 0, (-(far + near) / (far - near)), -1.0f,
	         0, 0, ((-2*far*near) / (far - near)), 0     };
}

inline Mat4
ortho_matrix(float left, float right, float bottom, float top)
{
	return { 2.0f / (right - left), 0, 0, 0,
	         0, 2.0f / (top - bottom), 0, 0,
	         0, 0, -1.0f, 0,
	         -(right + left) / (right - left), -(top + bottom) / (top - bottom), 0, 0 };
}

// Assumes z is a unit vector.
inline Mat4
view_matrix(Vec3f eye, Vec3f zaxis)
{
	//assert(length(front) <= 1.0f);
	//zaxis = normalize(zaxis);
	Vec3f xaxis = normalize(cross_product(zaxis, { 0.0f, 1.0f, 0.0f }));
	Vec3f yaxis = cross_product(xaxis, zaxis);

	return { xaxis.x, yaxis.x, -zaxis.x, 0,
	         xaxis.y, yaxis.y, -zaxis.y, 0,
	         xaxis.z, yaxis.z, -zaxis.z, 0,
	         -dot_product(xaxis, eye), -dot_product(yaxis, eye), dot_product(zaxis, eye), 1 };
}

inline Mat4
transpose(Mat4 m)
{
	return { m[0], m[4], m[8],  m[12],
	         m[1], m[5], m[9],  m[13],
	         m[2], m[6], m[10], m[14],
	         m[3], m[7], m[11], m[15] };
}

inline Mat4
translate(Mat4 m, Vec3f pos)
{
	m(1,4) = pos.x;
	m(2,4) = pos.y;
	m(3,4) = pos.z;
	return m;
}

inline Mat4
make_mat4()
{
	return { 1, 0, 0, 0,
	         0, 1, 0, 0,
	         0, 0, 1, 0,
	         0, 0, 0, 1 };
}

inline Vec4f
operator*(Mat4 m, Vec4f v)
{
	Vec4f result;
	result.x = v.x*m[0] + v.y*m[4] + v.z*m[8]  + v.w*m[12];
	result.y = v.x*m[1] + v.y*m[5] + v.z*m[9]  + v.w*m[13];
	result.z = v.x*m[2] + v.y*m[6] + v.z*m[10] + v.w*m[14];
	result.w = v.x*m[3] + v.y*m[7] + v.z*m[11] + v.w*m[15];
	return result;
}

inline Vec3f
unproject(Vec3f win, const Mat4 &inverse_proj_view)
{
	Vec4f world_ray = inverse_proj_view * (Vec4f){ win.x, win.y, win.z, 1.0f };
	world_ray /= world_ray.w;
	return { world_ray.x, world_ray.y, world_ray.z };
}

inline Mat4
operator*(Mat4 a, Mat4 b)
{
	Mat4 res;
	for (int i = 0; i < 4; ++i) {
		float a1 = a(i,1), a2 = a(i,2), a3 = a(i,3), a4 = a(i,4);
		res(i,1) = a1*b(1,1) + a2*b(2,1) + a3*b(3,1) + a4*b(4,1);
		res(i,2) = a1*b(1,2) + a2*b(2,2) + a3*b(3,2) + a4*b(4,2);
		res(i,3) = a1*b(1,3) + a2*b(2,3) + a3*b(3,3) + a4*b(4,3);
		res(i,4) = a1*b(1,4) + a2*b(2,4) + a3*b(3,4) + a4*b(4,4);
	}
	/*
	result.m11 = a.m11*b.m11 + a.m12*b.m21 + a.m13*b.m31 + a.m14*b.m41;
	result.m21 = a.m21*b.m11 + a.m22*b.m21 + a.m23*b.m31 + a.m24*b.m41;
	result.m31 = a.m31*b.m11 + a.m32*b.m21 + a.m33*b.m31 + a.m34*b.m41;
	result.m41 = a.m41*b.m11 + a.m42*b.m21 + a.m43*b.m31 + a.m44*b.m41;

	result.m12 = a.m11*b.m12 + a.m12*b.m22 + a.m13*b.m32 + a.m14*b.m42;
	result.m22 = a.m21*b.m12 + a.m22*b.m22 + a.m23*b.m32 + a.m24*b.m42;
	result.m32 = a.m31*b.m12 + a.m32*b.m22 + a.m33*b.m32 + a.m34*b.m42;
	result.m42 = a.m41*b.m12 + a.m42*b.m22 + a.m43*b.m32 + a.m44*b.m42;

	result.m13 = a.m11*b.m13 + a.m12*b.m23 + a.m13*b.m33 + a.m14*b.m43;
	result.m23 = a.m21*b.m13 + a.m22*b.m23 + a.m23*b.m33 + a.m24*b.m43;
	result.m33 = a.m31*b.m13 + a.m32*b.m23 + a.m33*b.m33 + a.m34*b.m43;
	result.m43 = a.m41*b.m13 + a.m42*b.m23 + a.m43*b.m33 + a.m44*b.m43;

	result.m14 = a.m11*b.m14 + a.m12*b.m24 + a.m13*b.m34 + a.m14*b.m44;
	result.m24 = a.m21*b.m14 + a.m22*b.m24 + a.m23*b.m34 + a.m24*b.m44;
	result.m34 = a.m31*b.m14 + a.m32*b.m24 + a.m33*b.m34 + a.m34*b.m44;
	result.m44 = a.m41*b.m14 + a.m42*b.m24 + a.m43*b.m34 + a.m44*b.m44;
*/
	return res;
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
inverse(Mat4 m)
{
	Mat4 inv;
	inv[0] = m[5]  * m[10] * m[15] - 
	         m[5]  * m[11] * m[14] - 
	         m[9]  * m[6]  * m[15] + 
	         m[9]  * m[7]  * m[14] +
	         m[13] * m[6]  * m[11] - 
	         m[13] * m[7]  * m[10];

	inv[4] = -m[4]  * m[10] * m[15] + 
	          m[4]  * m[11] * m[14] + 
	          m[8]  * m[6]  * m[15] - 
	          m[8]  * m[7]  * m[14] - 
	          m[12] * m[6]  * m[11] + 
	          m[12] * m[7]  * m[10];

	inv[8] = m[4]  * m[9] * m[15] - 
	         m[4]  * m[11] * m[13] - 
	         m[8]  * m[5] * m[15] + 
	         m[8]  * m[7] * m[13] + 
	         m[12] * m[5] * m[11] - 
	         m[12] * m[7] * m[9];

	inv[12] = -m[4]  * m[9] * m[14] + 
	           m[4]  * m[10] * m[13] +
	           m[8]  * m[5] * m[14] - 
	           m[8]  * m[6] * m[13] - 
	           m[12] * m[5] * m[10] + 
	           m[12] * m[6] * m[9];

	inv[1] = -m[1]  * m[10] * m[15] + 
	          m[1]  * m[11] * m[14] + 
	          m[9]  * m[2] * m[15] - 
	          m[9]  * m[3] * m[14] - 
	          m[13] * m[2] * m[11] + 
	          m[13] * m[3] * m[10];

	inv[5] = m[0]  * m[10] * m[15] - 
	         m[0]  * m[11] * m[14] - 
	         m[8]  * m[2] * m[15] + 
	         m[8]  * m[3] * m[14] + 
	         m[12] * m[2] * m[11] - 
	         m[12] * m[3] * m[10];

	inv[9] = -m[0]  * m[9] * m[15] + 
	          m[0]  * m[11] * m[13] + 
	          m[8]  * m[1] * m[15] - 
	          m[8]  * m[3] * m[13] - 
	          m[12] * m[1] * m[11] + 
	          m[12] * m[3] * m[9];

	inv[13] = m[0]  * m[9] * m[14] - 
	          m[0]  * m[10] * m[13] - 
	          m[8]  * m[1] * m[14] + 
	          m[8]  * m[2] * m[13] + 
	          m[12] * m[1] * m[10] - 
	          m[12] * m[2] * m[9];

	inv[2] = m[1]  * m[6] * m[15] - 
	         m[1]  * m[7] * m[14] - 
	         m[5]  * m[2] * m[15] + 
	         m[5]  * m[3] * m[14] + 
	         m[13] * m[2] * m[7] - 
	         m[13] * m[3] * m[6];

	inv[6] = -m[0]  * m[6] * m[15] + 
	          m[0]  * m[7] * m[14] + 
	          m[4]  * m[2] * m[15] - 
	          m[4]  * m[3] * m[14] - 
	          m[12] * m[2] * m[7] + 
	          m[12] * m[3] * m[6];

	inv[10] = m[0]  * m[5] * m[15] - 
	          m[0]  * m[7] * m[13] - 
	          m[4]  * m[1] * m[15] + 
	          m[4]  * m[3] * m[13] + 
	          m[12] * m[1] * m[7] - 
	          m[12] * m[3] * m[5];

	inv[14] = -m[0]  * m[5] * m[14] + 
	           m[0]  * m[6] * m[13] + 
	           m[4]  * m[1] * m[14] - 
	           m[4]  * m[2] * m[13] - 
	           m[12] * m[1] * m[6] + 
	           m[12] * m[2] * m[5];

	inv[3] = -m[1] * m[6] * m[11] + 
	          m[1] * m[7] * m[10] + 
	          m[5] * m[2] * m[11] - 
	          m[5] * m[3] * m[10] - 
	          m[9] * m[2] * m[7] + 
	          m[9] * m[3] * m[6];

	inv[7] = m[0] * m[6] * m[11] - 
	         m[0] * m[7] * m[10] - 
	         m[4] * m[2] * m[11] + 
	         m[4] * m[3] * m[10] + 
	         m[8] * m[2] * m[7] - 
	         m[8] * m[3] * m[6];

	inv[11] = -m[0] * m[5] * m[11] + 
	           m[0] * m[7] * m[9] + 
	           m[4] * m[1] * m[11] - 
	           m[4] * m[3] * m[9] - 
	           m[8] * m[1] * m[7] + 
	           m[8] * m[3] * m[5];

	inv[15] = m[0] * m[5] * m[10] - 
	          m[0] * m[6] * m[9] - 
	          m[4] * m[1] * m[10] + 
	          m[4] * m[2] * m[9] + 
	          m[8] * m[1] * m[6] - 
	          m[8] * m[2] * m[5];

	float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

	if (det == 0) {
		assert(0); // TEMP
		return {};
	}

	det = 1.0 / det;

	for (int i = 0; i < 16; i++)
		inv[i] = inv[i] * det;

	return inv;
}

#endif
