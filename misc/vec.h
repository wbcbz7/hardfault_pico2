#ifndef __VEC_H
#define __VEC_H

#include <math.h>
#include <stdint.h>
#include "fxmath.h"

// well known inverse sqrt from quake 3 source :)
inline float Q_rsqrt( float number ){
	long i;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;                       // evil floating point bit level hacking
	i  = 0x5f375a86 - ( i >> 1 );               // what the fuck? (more accurate constant also)
	y  = * ( float * ) &i;
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
	//y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
}

// ----------------------------------

struct vec2i;
struct vec2f;
struct vec2x;
struct vec3f;
struct vec3x;
struct vec4f;
struct vec4x;

// ----------------------------------
// integer
struct vec2i {
    signed long x;
    signed long y;
    
    // add/subtract another vec2i
    inline vec2i& operator+=(const vec2i & rhs) {x += rhs.x; y += rhs.y; return *this;}
    inline vec2i& operator-=(const vec2i & rhs) {x -= rhs.x; y -= rhs.y; return *this;}
    
    // multiply/divide by scalar
    inline vec2i& operator*=(const signed rhs) {x *= rhs;   y *= rhs;   return *this;}
    inline vec2i& operator/=(const signed rhs) {x /= rhs;   y /= rhs;   return *this;}
    inline vec2i& operator*=(const float  rhs) {x *= rhs;   y *= rhs;   return *this;}
    inline vec2i& operator/=(const float  rhs) {x /= rhs;   y /= rhs;   return *this;}
    
    
    // signs and other stuff
    inline vec2i& operator-() {x = -x; y = -y; return *this;}
    
    // add/subtract two vec2f
    inline const vec2i operator+(const vec2i& rhs) {return vec2i(*this) += rhs;}
    inline const vec2i operator-(const vec2i& rhs) {return vec2i(*this) -= rhs;}
    
    // multiply/divide by scalar and throw to another vec2i
    inline const vec2i operator*(const signed rhs) {return vec2i(*this) *= rhs;}
    inline const vec2i operator/(const signed rhs) {return vec2i(*this) /= rhs;}
    
    inline operator vec2f();
    inline operator vec2x();
};

// dot product
inline signed long dot (const vec2i& lhs, const vec2i& rhs) {return (lhs.x*rhs.x + lhs.y*rhs.y);}
// length
inline signed long abs (const vec2i& lhs) {return sqrt(lhs.x*lhs.x + lhs.y*lhs.y);}
// norm
inline vec2i norm(const vec2i& lhs) {vec2i r = lhs; return (r / abs(r));}

// ----------------------------------

struct vec2f {
    float x;
    float y;
    
    // add/subtract another vec2f
    inline vec2f& operator+=(const vec2f & rhs) {x += rhs.x; y += rhs.y; return *this;}
    inline vec2f& operator-=(const vec2f & rhs) {x -= rhs.x; y -= rhs.y; return *this;}
    
    // multiply/divide by scalar
    inline vec2f& operator*=(const signed rhs) {x *= rhs;   y *= rhs;   return *this;}
    inline vec2f& operator/=(const signed rhs) {x /= rhs;   y /= rhs;   return *this;}
    inline vec2f& operator*=(const float  rhs) {x *= rhs;   y *= rhs;   return *this;}
    inline vec2f& operator/=(const float  rhs) {x /= rhs;   y /= rhs;   return *this;}
    
    // signs and other stuff
    inline vec2f& operator-() {x = -x; y = -y; return *this;}
    
    // add/subtract two vec2f
    inline const vec2f operator+(const vec2f& rhs) {return vec2f(*this) += rhs;}
    inline const vec2f operator-(const vec2f& rhs) {return vec2f(*this) -= rhs;}

    // multiply/divide by scalar and throw to another vec2f
    inline const vec2f operator*(const signed rhs) {return vec2f(*this) *= rhs;}
    inline const vec2f operator/(const signed rhs) {return vec2f(*this) /= rhs;}
    inline const vec2f operator*(const float  rhs) {return vec2f(*this) *= rhs;}
    inline const vec2f operator/(const float  rhs) {return vec2f(*this) /= rhs;}
    
    inline operator vec2i();
    inline operator vec2x();
};

// watcom "optimized" stuff
inline void add2f(vec2f& r, const vec2f &lhs, const int32_t &rhs) { r.x = lhs.x + rhs; r.y = lhs.y + rhs; }
inline void sub2f(vec2f& r, const vec2f &lhs, const int32_t &rhs) { r.x = lhs.x - rhs; r.y = lhs.y - rhs; }
inline void add2f(vec2f& r, const vec2f &lhs, const float &rhs) { r.x = lhs.x + rhs; r.y = lhs.y + rhs; }
inline void sub2f(vec2f& r, const vec2f &lhs, const float &rhs) { r.x = lhs.x - rhs; r.y = lhs.y - rhs; }
inline void add2f(vec2f& r, const vec2f &lhs, const vec2f &rhs) { r.x = lhs.x + rhs.x; r.y = lhs.y + rhs.y; }
inline void sub2f(vec2f& r, const vec2f &lhs, const vec2f &rhs) { r.x = lhs.x - rhs.x; r.y = lhs.y - rhs.y; }
inline void mul2f(vec2f& r, const vec2f &lhs, const vec2f &rhs) { r.x = lhs.x * rhs.x; r.y = lhs.y * rhs.y; }
inline void div2f(vec2f& r, const vec2f &lhs, const vec2f &rhs) { r.x = lhs.x / rhs.x; r.y = lhs.y / rhs.y; }
inline void mul2f(vec2f& r, const vec2f &lhs, const int32_t &rhs) { r.x = lhs.x * rhs; r.y = lhs.y * rhs; }
inline void div2f(vec2f& r, const vec2f &lhs, const int32_t &rhs) { r.x = lhs.x / rhs; r.y = lhs.y / rhs; }
inline void mul2f(vec2f& r, const vec2f &lhs, const float &rhs) { r.x = lhs.x * rhs; r.y = lhs.y * rhs; }
inline void div2f(vec2f& r, const vec2f &lhs, const float &rhs) { r.x = lhs.x / rhs; r.y = lhs.y / rhs; }
inline void neg2f(vec2f& r, const vec2f &lhs) { r.x = -lhs.x; r.y = -lhs.y; }

// dot product
inline float dot (const vec2f& lhs, const vec2f& rhs) {return (lhs.x*rhs.x + lhs.y*rhs.y);}
// length
inline float abs (const vec2f& lhs) {return sqrt(lhs.x*lhs.x + lhs.y*lhs.y);}
inline float abs2(const vec2f& lhs) {return lhs.x*lhs.x + lhs.y*lhs.y;}
// norm
inline void  norm(vec2f& r, const vec2f& lhs) {
    float len = Q_rsqrt(lhs.x*lhs.x + lhs.y*lhs.y);
    r.x = lhs.x * len;
    r.y = lhs.y * len;
}
inline vec2f norm(const vec2f& lhs) {vec2f r; norm(r, lhs); return r;}

// ----------------------------------
// fixed point 16:16
struct vec2x {
    signed long x;
    signed long y;
    
    // add/subtract another vec2f
    inline vec2x& operator+=(const vec2x & rhs) {x += rhs.x; y += rhs.y; return *this;}
    inline vec2x& operator-=(const vec2x & rhs) {x -= rhs.x; y -= rhs.y; return *this;}
    
    // multiply/divide by scalar
    inline vec2x& operator*=(const signed rhs) {x *= rhs; y *= rhs;   return *this;}
    inline vec2x& operator/=(const signed rhs) {x /= rhs; y /= rhs;   return *this;}
    
    inline vec2x& operator*=(const float  rhs) {
        x = imul16(x, (signed long)(rhs*65536.0f));
        y = imul16(y, (signed long)(rhs*65536.0f));
        return *this;
    }
    inline vec2x& operator/=(const float  rhs) {
        x = idiv16(x, (signed long)(rhs*65536.0f));
        y = idiv16(y, (signed long)(rhs*65536.0f));
        return *this;
    }
    
    // signs and other stuff
    inline vec2x& operator-() {x = -x; y = -y; return *this;}
    
    // add/subtract two vec2f
    inline const vec2x operator+(const vec2x& rhs) {return vec2x(*this) += rhs;}
    inline const vec2x operator-(const vec2x& rhs) {return vec2x(*this) -= rhs;}
    
    // multiply/divide by scalar and throw to another vec2f
    inline const vec2x operator*(const signed rhs) {return vec2x(*this) *= rhs;}
    inline const vec2x operator/(const signed rhs) {return vec2x(*this) /= rhs;}
    inline const vec2x operator*(const float  rhs) {return vec2x(*this) *= rhs;}
    inline const vec2x operator/(const float  rhs) {return vec2x(*this) /= rhs;}
    
    inline operator vec2f();
    inline operator vec2i();
};

// watcom "optimized" stuff
inline void add2x(vec2x& r, const vec2x &lhs, const int32_t &rhs) { r.x = lhs.x + rhs; r.y = lhs.y + rhs; }
inline void sub2x(vec2x& r, const vec2x &lhs, const int32_t &rhs) { r.x = lhs.x - rhs; r.y = lhs.y - rhs; }
inline void add2x(vec2x& r, const vec2x &lhs, const float &rhs) { r.x = lhs.x + (int32_t)(rhs*65536.0f); r.y = lhs.y + (int32_t)(rhs*65536.0f); }
inline void sub2x(vec2x& r, const vec2x &lhs, const float &rhs) { r.x = lhs.x - (int32_t)(rhs*65536.0f); r.y = lhs.y - (int32_t)(rhs*65536.0f); }
inline void add2x(vec2x& r, const vec2x &lhs, const vec2x &rhs) { r.x = lhs.x + rhs.x; r.y = lhs.y + rhs.y; }
inline void sub2x(vec2x& r, const vec2x &lhs, const vec2x &rhs) { r.x = lhs.x - rhs.x; r.y = lhs.y - rhs.y; }
inline void mul2x(vec2x& r, const vec2x &lhs, const int32_t &rhs) { r.x = lhs.x * rhs; r.y = lhs.y * rhs; }
inline void div2x(vec2x& r, const vec2x &lhs, const int32_t &rhs) { r.x = lhs.x / rhs; r.y = lhs.y / rhs; }
inline void mul2x(vec2x& r, const vec2x &lhs, const float &rhs) { r.x = imul16(lhs.x, (int32_t)(rhs*65536.0f)); r.y = imul16(lhs.y, (int32_t)(rhs*65536.0f)); }
inline void div2x(vec2x& r, const vec2x &lhs, const float &rhs) { r.x = idiv16(lhs.x, (int32_t)(rhs*65536.0f)); r.y = idiv16(lhs.y, (int32_t)(rhs*65536.0f)); }
inline void mul2x(vec2x& r, const vec2x &lhs, const vec2x &rhs) { r.x = imul16(lhs.x, rhs.x); r.y = imul16(lhs.y, rhs.y); }
inline void div2x(vec2x& r, const vec2x &lhs, const vec2x &rhs) { r.x = idiv16(lhs.x, rhs.x); r.y = idiv16(lhs.y, rhs.y); }
inline void neg2x(vec2x& r, const vec2x &lhs) { r.x = -lhs.x; r.y = -lhs.y; }

// dot product
inline int32_t dot (const vec2x& lhs, const vec2x& rhs) {return (imul16(lhs.x, rhs.x) + imul16(lhs.y, rhs.y));}
// length
inline float abs (const vec2x& lhs) {return sqrt(imul16(lhs.x, lhs.x) + imul16(lhs.y, lhs.y));}
// norm
inline void  norm(vec2x &r, const vec2x& lhs) {
    float len = 1.0 / abs(lhs);
    r.x = lhs.x * len;
    r.y = lhs.y * len;
};
inline vec2x norm(const vec2x& lhs) {vec2x r = lhs; return (r / abs(r));}


// type conversions

inline vec2f::operator vec2x() {vec2x r; fist(&r.x, x*65536.0f); fist(&r.y, y*65536.0f); return r;}
inline vec2f::operator vec2i() {vec2i r = {(signed long)x, (signed long)y}; return r;}
inline vec2x::operator vec2f() {vec2f r = {((float)x / 65536.0f), ((float)y / 65536.0f)}; return r;}
inline vec2x::operator vec2i() {vec2i r = {(x >> 16), (x >> 16)}; return r;}

// ----------------------------------

struct vec3f {
    float x;
    float y;
    float z;
    
    // add/subtract another vec2f
    inline vec3f& operator+=(const vec3f & rhs) {x += rhs.x; y += rhs.y; z += rhs.z; return *this;}
    inline vec3f& operator-=(const vec3f & rhs) {x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this;}
    
    // multiply/divide by scalar
    inline vec3f& operator*=(const signed rhs) {x *= rhs; y *= rhs; z *= rhs; return *this;}
    inline vec3f& operator/=(const signed rhs) {x /= rhs; y /= rhs; z /= rhs; return *this;}
    inline vec3f& operator*=(const float  rhs) {x *= rhs; y *= rhs; z *= rhs; return *this;}
    inline vec3f& operator/=(const float  rhs) {x /= rhs; y /= rhs; z /= rhs; return *this;}
    
    inline vec3f& operator*=(signed &rhs) {x *= rhs; y *= rhs; z *= rhs; return *this;}
    inline vec3f& operator/=(signed &rhs) {x /= rhs; y /= rhs; z /= rhs; return *this;}
    inline vec3f& operator*=(float  &rhs) {x *= rhs; y *= rhs; z *= rhs; return *this;}
    inline vec3f& operator/=(float  &rhs) {x /= rhs; y /= rhs; z /= rhs; return *this;}
    
    // signs and other stuff
    inline vec3f& operator-() {x = -x; y = -y; z = -z; return *this;}
    
    // add/subtract two vec2f
    inline const vec3f operator+(const vec3f& rhs) {return vec3f(*this) += rhs;}
    inline const vec3f operator-(const vec3f& rhs) {return vec3f(*this) -= rhs;}
    
    // multiply/divide by scalar and throw to another vec2f
    inline const vec3f operator*(const signed rhs) {return vec3f(*this) *= rhs;}
    inline const vec3f operator/(const signed rhs) {return vec3f(*this) /= rhs;}
    inline const vec3f operator*(const float  rhs) {return vec3f(*this) *= rhs;}
    inline const vec3f operator/(const float  rhs) {return vec3f(*this) /= rhs;}
    
    inline operator vec3x();
};

// watcom "optimized" stuff
inline void add3f(vec3f& r, const vec3f &lhs, const int32_t &rhs) { r.x = lhs.x + rhs; r.y = lhs.y + rhs; r.z = lhs.z + rhs; }
inline void sub3f(vec3f& r, const vec3f &lhs, const int32_t &rhs) { r.x = lhs.x - rhs; r.y = lhs.y - rhs; r.z = lhs.z - rhs; }
inline void add3f(vec3f& r, const vec3f &lhs, const float &rhs) { r.x = lhs.x + rhs; r.y = lhs.y + rhs; r.z = lhs.z + rhs; }
inline void sub3f(vec3f& r, const vec3f &lhs, const float &rhs) { r.x = lhs.x - rhs; r.y = lhs.y - rhs; r.z = lhs.z - rhs; }
inline void add3f(vec3f& r, const vec3f &lhs, const vec3f &rhs) { r.x = lhs.x + rhs.x; r.y = lhs.y + rhs.y; r.z = lhs.z + rhs.z; }
inline void sub3f(vec3f& r, const vec3f &lhs, const vec3f &rhs) { r.x = lhs.x - rhs.x; r.y = lhs.y - rhs.y; r.z = lhs.z - rhs.z; }
inline void mul3f(vec3f& r, const vec3f &lhs, const int32_t &rhs) { r.x = lhs.x * rhs; r.y = lhs.y * rhs; r.z = lhs.z * rhs; }
inline void div3f(vec3f& r, const vec3f &lhs, const int32_t &rhs) { r.x = lhs.x / rhs; r.y = lhs.y / rhs; r.z = lhs.z / rhs; }
inline void mul3f(vec3f& r, const vec3f &lhs, const float &rhs) { r.x = lhs.x * rhs; r.y = lhs.y * rhs; r.z = lhs.z * rhs; }
inline void div3f(vec3f& r, const vec3f &lhs, const float &rhs) { r.x = lhs.x / rhs; r.y = lhs.y / rhs; r.z = lhs.z / rhs; }
inline void mul3f(vec3f& r, const vec3f &lhs, const vec3f &rhs) { r.x = lhs.x * rhs.x; r.y = lhs.y * rhs.y; r.z = lhs.z * rhs.z; }
inline void div3f(vec3f& r, const vec3f &lhs, const vec3f &rhs) { r.x = lhs.x / rhs.x; r.y = lhs.y / rhs.y; r.z = lhs.z / rhs.z; }
inline void neg3f(vec3f& r, const vec3f &lhs) { r.x = -lhs.x; r.y = -lhs.y;  r.z = -lhs.z; }

// dot product
inline float dot  (const vec3f& lhs, const vec3f& rhs) {return (lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z);}
// cross product
inline void cross(vec3f &r, const vec3f& lhs, const vec3f& rhs) {
    r.x = lhs.y * rhs.z - lhs.z * rhs.y;
    r.y = lhs.z * rhs.x - lhs.x * rhs.z;
    r.z = lhs.x * rhs.y - lhs.y * rhs.x;
}
inline vec3f cross(const vec3f& lhs, const vec3f& rhs) {
    vec3f r;
    cross(r, lhs, rhs);
    return r;
}
// length
inline float abs  (const vec3f& lhs) {return sqrt(lhs.x*lhs.x + lhs.y*lhs.y + lhs.z*lhs.z);}
// norm
inline void  norm(vec3f& r, const vec3f& lhs) {
    float len = Q_rsqrt(lhs.x*lhs.x + lhs.y*lhs.y + lhs.z*lhs.z);
    r.x = lhs.x * len;
    r.y = lhs.y * len;
    r.z = lhs.z * len;
}
inline vec3f norm(const vec3f& lhs) {vec3f r; norm(r, lhs); return r;}
// more correct norm
inline void  normsq(vec3f& r, const vec3f& lhs) {
    float len = 1.0 / sqrt(lhs.x*lhs.x + lhs.y*lhs.y + lhs.z*lhs.z);
    r.x = lhs.x * len;
    r.y = lhs.y * len;
    r.z = lhs.z * len;
}
inline vec3f normsq(const vec3f& lhs) {vec3f r; normsq(r, lhs); return r;}


// ----------------------------------
// fixed point 16:16
struct vec3x {
    signed long x;
    signed long y;
    signed long z;
    
    // add/subtract another vec2f
    inline vec3x& operator+=(const vec3x & rhs) {x += rhs.x; y += rhs.y; z += rhs.z; return *this;}
    inline vec3x& operator-=(const vec3x & rhs) {x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this;}
    
    // multiply/divide by scalar
    inline vec3x& operator*=(const float  rhs) {
        x = imul16(x, (signed long)(rhs*65536.0f));
        y = imul16(y, (signed long)(rhs*65536.0f));
        z = imul16(z, (signed long)(rhs*65536.0f));
        return *this;
    }
    inline vec3x& operator/=(const float  rhs) {
        x = idiv16(x, (signed long)(rhs*65536.0f));
        y = idiv16(y, (signed long)(rhs*65536.0f));
        z = idiv16(z, (signed long)(rhs*65536.0f));
        return *this;
    }
    
    // signs and other stuff
    inline vec3x& operator-() {x = -x; y = -y; z = -z; return *this;}
    
    // add/subtract two vec2f
    inline const vec3x operator+(const vec3x& rhs) {return vec3x(*this) += rhs;}
    inline const vec3x operator-(const vec3x& rhs) {return vec3x(*this) -= rhs;}
    
    // multiply/divide by scalar and throw to another vec2f
    inline const vec3x operator*(const signed rhs) {return vec3x(*this) *= rhs;}
    inline const vec3x operator/(const signed rhs) {return vec3x(*this) /= rhs;}
    inline const vec3x operator*(const float  rhs) {return vec3x(*this) *= rhs;}
    inline const vec3x operator/(const float  rhs) {return vec3x(*this) /= rhs;}
    
    inline operator vec3f();
};

// watcom "optimized" stuff
inline void add3x(vec3x& r, const vec3x &lhs, const int32_t &rhs) { r.x = lhs.x + rhs; r.y = lhs.y + rhs; r.z = lhs.z + rhs; }
inline void sub3x(vec3x& r, const vec3x &lhs, const int32_t &rhs) { r.x = lhs.x - rhs; r.y = lhs.y - rhs; r.z = lhs.z - rhs; }
inline void add3x(vec3x& r, const vec3x &lhs, const float &rhs) { r.x = lhs.x + (int32_t)(rhs*65536.0f); r.y = lhs.y + (int32_t)(rhs*65536.0f); r.z = lhs.z + (int32_t)(rhs*65536.0f); }
inline void sub3x(vec3x& r, const vec3x &lhs, const float &rhs) { r.x = lhs.x - (int32_t)(rhs*65536.0f); r.y = lhs.y - (int32_t)(rhs*65536.0f); r.z = lhs.z - (int32_t)(rhs*65536.0f); }
inline void add3x(vec3x& r, const vec3x &lhs, const vec3x &rhs) { r.x = lhs.x + rhs.x; r.y = lhs.y + rhs.y; r.z = lhs.z + rhs.z; }
inline void sub3x(vec3x& r, const vec3x &lhs, const vec3x &rhs) { r.x = lhs.x - rhs.x; r.y = lhs.y - rhs.y; r.z = lhs.z - rhs.z; }
inline void mul3x(vec3x& r, const vec3x &lhs, const int32_t &rhs) { r.x = lhs.x * rhs; r.y = lhs.y * rhs; r.z = lhs.z * rhs; }
inline void div3x(vec3x& r, const vec3x &lhs, const int32_t &rhs) { r.x = lhs.x / rhs; r.y = lhs.y / rhs; r.z = lhs.z / rhs; }
inline void mul3x(vec3x& r, const vec3x &lhs, const float &rhs) { r.x = imul16(lhs.x, (int32_t)(rhs*65536.0f)); r.y = imul16(lhs.y, (int32_t)(rhs*65536.0f)); r.z = imul16(lhs.z, (int32_t)(rhs*65536.0f)); }
inline void div3x(vec3x& r, const vec3x &lhs, const float &rhs) { r.x = idiv16(lhs.x, (int32_t)(rhs*65536.0f)); r.y = idiv16(lhs.y, (int32_t)(rhs*65536.0f)); r.z = idiv16(lhs.z, (int32_t)(rhs*65536.0f)); }
inline void mul3x(vec3x& r, const vec3x &lhs, const vec3x &rhs) { r.x = imul16(lhs.x, rhs.x); r.y = imul16(lhs.y, rhs.y); r.z = imul16(lhs.z, rhs.z); }
inline void div3x(vec3x& r, const vec3x &lhs, const vec3x &rhs) { r.x = idiv16(lhs.x, rhs.x); r.y = idiv16(lhs.y, rhs.y); r.z = idiv16(lhs.z, rhs.z); }
inline void neg3x(vec3x& r, const vec3x &lhs) { r.x = -lhs.x; r.y = -lhs.y;  r.z = -lhs.z; }

// dot product
inline float dot  (const vec3x& lhs, const vec3x& rhs) {return (imul16(lhs.x, rhs.x) + imul16(lhs.y, rhs.y) + imul16(lhs.z, rhs.z));}
// cross product
inline void cross(vec3x &r, const vec3x& lhs, const vec3x& rhs) {
    r.x = imul16(lhs.y, rhs.z) - imul16(lhs.z, rhs.y);
    r.y = imul16(lhs.z, rhs.x) - imul16(lhs.x, rhs.z);
    r.z = imul16(lhs.x, rhs.y) - imul16(lhs.y, rhs.x);
}
inline vec3x cross(const vec3x& lhs, const vec3x& rhs) {
    vec3x r;
    cross(r, lhs, rhs);
    return r;
}
// length
inline float abs  (const vec3x& lhs) {return sqrt(imul16(lhs.x, lhs.x) + imul16(lhs.y, lhs.y) + imul16(lhs.z, lhs.z));}
// norm
inline void  norm(vec3x& r, const vec3x& lhs) {
    float len = sqrt(imul16(lhs.x, lhs.x) + imul16(lhs.y, lhs.y) + imul16(lhs.z, lhs.z)) * 256.0f;
    r.x = lhs.x * len;
    r.y = lhs.y * len;
    r.z = lhs.z * len;
}
inline vec3x norm(const vec3x& lhs) {vec3x r = lhs; return (r / abs(r));}

// type conversions
inline vec3f::operator vec3x() {vec3x r; fist(&r.x, x*65536.0f); fist(&r.y, y*65536.0f); fist(&r.z, z*65536.0f); return r;}
inline vec3x::operator vec3f() {vec3f r = {((float)x / 65536.0f), ((float)y / 65536.0f), ((float)z / 65536.0f)}; return r;}

// ----------------------------------

// in fact these are homogenous vectors, i.e. most R4 vector rules are heavily simplified
struct vec4f {
    union {float x; float b;};
    union {float y; float g;};
    union {float z; float r;};
    union {float w; float a; int32_t iw;};
    
    // add/subtract another vec2f
    inline vec4f& operator+=(const vec4f & rhs) {x += rhs.x; y += rhs.y; z += rhs.z; return *this;}
    inline vec4f& operator-=(const vec4f & rhs) {x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this;}
    
    // multiply/divide by scalar
    inline vec4f& operator*=(const signed rhs) {x *= rhs; y *= rhs; z *= rhs; return *this;}
    inline vec4f& operator/=(const signed rhs) {x /= rhs; y /= rhs; z /= rhs; return *this;}
    inline vec4f& operator*=(const float  rhs) {x *= rhs; y *= rhs; z *= rhs; return *this;}
    inline vec4f& operator/=(const float  rhs) {x /= rhs; y /= rhs; z /= rhs; return *this;}
    
    // signs and other stuff
    inline vec4f& operator-() {x = -x; y = -y; z = -z; return *this;}
    
    // add/subtract two vec2f
    inline const vec4f operator+(const vec4f& rhs) {return vec4f(*this) += rhs;}
    inline const vec4f operator-(const vec4f& rhs) {return vec4f(*this) -= rhs;}
    
    // multiply/divide by scalar and throw to another vec2f
    inline const vec4f operator*(const signed rhs) {return vec4f(*this) *= rhs;}
    inline const vec4f operator/(const signed rhs) {return vec4f(*this) /= rhs;}
    inline const vec4f operator*(const float  rhs) {return vec4f(*this) *= rhs;}
    inline const vec4f operator/(const float  rhs) {return vec4f(*this) /= rhs;}
  
    inline operator vec4x();  
};

// watcom "optimized" stuff
inline void add4f(vec4f& r, const vec4f &lhs, const int32_t &rhs) { r.x = lhs.x + rhs; r.y = lhs.y + rhs; r.z = lhs.z + rhs; }
inline void sub4f(vec4f& r, const vec4f &lhs, const int32_t &rhs) { r.x = lhs.x - rhs; r.y = lhs.y - rhs; r.z = lhs.z - rhs; }
inline void add4f(vec4f& r, const vec4f &lhs, const float &rhs) { r.x = lhs.x + rhs; r.y = lhs.y + rhs; r.z = lhs.z + rhs; }
inline void sub4f(vec4f& r, const vec4f &lhs, const float &rhs) { r.x = lhs.x - rhs; r.y = lhs.y - rhs; r.z = lhs.z - rhs; }
inline void add4f(vec4f& r, const vec4f &lhs, const vec4f &rhs) { r.x = lhs.x + rhs.x; r.y = lhs.y + rhs.y; r.z = lhs.z + rhs.z; }
inline void sub4f(vec4f& r, const vec4f &lhs, const vec4f &rhs) { r.x = lhs.x - rhs.x; r.y = lhs.y - rhs.y; r.z = lhs.z - rhs.z; }
inline void mul4f(vec4f& r, const vec4f &lhs, const int32_t &rhs) { r.x = lhs.x * rhs; r.y = lhs.y * rhs; r.z = lhs.z * rhs; }
inline void div4f(vec4f& r, const vec4f &lhs, const int32_t &rhs) { r.x = lhs.x / rhs; r.y = lhs.y / rhs; r.z = lhs.z / rhs; }
inline void mul4f(vec4f& r, const vec4f &lhs, const float &rhs) { r.x = lhs.x * rhs; r.y = lhs.y * rhs; r.z = lhs.z * rhs; }
inline void div4f(vec4f& r, const vec4f &lhs, const float &rhs) { r.x = lhs.x / rhs; r.y = lhs.y / rhs; r.z = lhs.z / rhs; }
inline void mul4f(vec4f& r, const vec4f &lhs, const vec4f &rhs) { r.x = lhs.x * rhs.x; r.y = lhs.y * rhs.y; r.z = lhs.z * rhs.z; }
inline void div4f(vec4f& r, const vec4f &lhs, const vec4f &rhs) { r.x = lhs.x / rhs.x; r.y = lhs.y / rhs.y; r.z = lhs.z / rhs.z; }
inline void neg4f(vec4f& r, const vec4f &lhs) { r.x = -lhs.x; r.y = -lhs.y;  r.z = -lhs.z; }

// dot product
inline float dot  (const vec4f& lhs, const vec4f& rhs) {return (lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z);}
// cross product
inline vec4f cross(const vec4f& lhs, const vec4f& rhs) {
    vec4f r;
    r.x = lhs.y * rhs.z - lhs.z * rhs.y;
    r.y = lhs.z * rhs.x - lhs.x * rhs.z;
    r.z = lhs.x * rhs.y - lhs.y * rhs.x;
    r.w = lhs.w;
    return r;
}
// length
inline float abs  (const vec4f& lhs) {return sqrt(lhs.x*lhs.x + lhs.y*lhs.y + lhs.z*lhs.z);}
// norm
inline void  norm(vec4f& r, const vec4f& lhs) {
    float len = Q_rsqrt(lhs.x*lhs.x + lhs.y*lhs.y + lhs.z*lhs.z);
    r.x = lhs.x * len;
    r.y = lhs.y * len;
    r.z = lhs.z * len;
    r.w = 1.0;
}
inline vec4f norm(const vec4f& lhs) {vec4f r; norm(r, lhs); return r;}
// more correct norm
inline void  normsq(vec4f& r, const vec4f& lhs) {
    float len = 1.0 / sqrt(lhs.x*lhs.x + lhs.y*lhs.y + lhs.z*lhs.z);
    r.x = lhs.x * len;
    r.y = lhs.y * len;
    r.z = lhs.z * len;
    r.w - 1.0;
}
inline vec4f normsq(const vec4f& lhs) {vec4f r; normsq(r, lhs); return r;}

// ----------------------------------
// fixed point 16:16
struct vec4x {
    signed long x;
    signed long y;
    signed long z;
    signed long w;
    
    // add/subtract another vec2f
    inline vec4x& operator+=(const vec4x & rhs) {x += rhs.x; y += rhs.y; z += rhs.z; return *this;}
    inline vec4x& operator-=(const vec4x & rhs) {x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this;}
    
    // multiply/divide by scalar
    inline vec4x& operator*=(const float  rhs) {
        x = imul16(x, (signed long)(rhs*65536.0f));
        y = imul16(y, (signed long)(rhs*65536.0f));
        z = imul16(z, (signed long)(rhs*65536.0f));
        return *this;
    }
    inline vec4x& operator/=(const float  rhs) {
        x = idiv16(x, (signed long)(rhs*65536.0f));
        y = idiv16(y, (signed long)(rhs*65536.0f));
        z = idiv16(z, (signed long)(rhs*65536.0f));
        return *this;
    }
    
    // signs and other stuff
    inline vec4x& operator-() {x = -x; y = -y; z = -z; return *this;}
    
    // add/subtract two vec2f
    inline const vec4x operator+(const vec4x& rhs) {return vec4x(*this) += rhs;}
    inline const vec4x operator-(const vec4x& rhs) {return vec4x(*this) -= rhs;}
    
    // multiply/divide by scalar and throw to another vec2f
    inline const vec4x operator*(const signed rhs) {return vec4x(*this) *= rhs;}
    inline const vec4x operator/(const signed rhs) {return vec4x(*this) /= rhs;}
    inline const vec4x operator*(const float  rhs) {return vec4x(*this) *= rhs;}
    inline const vec4x operator/(const float  rhs) {return vec4x(*this) /= rhs;}
    
    inline operator vec4f();
};

// dot product
inline float dot  (const vec4x& lhs, const vec4x& rhs) {return (imul16(lhs.x, rhs.x) + imul16(lhs.y, rhs.y) + imul16(lhs.z, rhs.z));}
// cross product
inline vec4x cross(const vec4x& lhs, const vec4x& rhs) {
    vec4f r;
    r.x = imul16(lhs.y, rhs.z) - imul16(lhs.z, rhs.y);
    r.y = imul16(lhs.z, rhs.x) - imul16(lhs.x, rhs.z);
    r.z = imul16(lhs.x, rhs.y) - imul16(lhs.y, rhs.x);
    r.w = lhs.w;
    return r;
}
// length
inline float abs  (const vec4x& lhs) {return sqrt(imul16(lhs.x, lhs.x) + imul16(lhs.y, lhs.y) + imul16(lhs.z, lhs.z));}
// norm
inline vec4x norm(const vec4x& lhs) {vec4x r = lhs; return (r / abs(r));}

// ----------------------------------
// integer
struct vec4i {
    signed long x;
    signed long y;
    signed long z;
    signed long w;
    
    // add/subtract another vec2f
    inline vec4i& operator+=(const vec4i & rhs) {x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w; return *this;}
    inline vec4i& operator-=(const vec4i & rhs) {x -= rhs.x; y -= rhs.y; z -= rhs.z; w += rhs.w; return *this;}
    
    // multiply/divide by scalar
    inline vec4i& operator*=(const float  rhs) {
        x *= rhs;
        y *= rhs;
        z *= rhs;
        w *= rhs;
        return *this;
    }
    inline vec4i& operator/=(const float  rhs) {
        x /= rhs;
        y /= rhs;
        z /= rhs;
        w /= rhs;
        return *this;
    }
    
    // signs and other stuff
    inline vec4i& operator-() {x = -x; y = -y; z = -z; w = -w; return *this;}
    
    // add/subtract two vec2f
    inline const vec4i operator+(const vec4i& rhs) {return vec4i(*this) += rhs;}
    inline const vec4i operator-(const vec4i& rhs) {return vec4i(*this) -= rhs;}
    
    // multiply/divide by scalar and throw to another vec2f
    inline const vec4i operator*(const signed rhs) {return vec4i(*this) *= rhs;}
    inline const vec4i operator/(const signed rhs) {return vec4i(*this) /= rhs;}
    inline const vec4i operator*(const float  rhs) {return vec4i(*this) *= rhs;}
    inline const vec4i operator/(const float  rhs) {return vec4i(*this) /= rhs;}
    
    inline operator vec4f();
};

// dot product
inline float dot  (const vec4i& lhs, const vec4i& rhs) {return (imul16(lhs.x, rhs.x) + imul16(lhs.y, rhs.y) + imul16(lhs.z, rhs.z));}
// cross product
inline vec4x cross(const vec4i& lhs, const vec4i& rhs) {
    vec4f r;
    r.x = imul16(lhs.y, rhs.z) - imul16(lhs.z, rhs.y);
    r.y = imul16(lhs.z, rhs.x) - imul16(lhs.x, rhs.z);
    r.z = imul16(lhs.x, rhs.y) - imul16(lhs.y, rhs.x);
    r.w = lhs.w;
    return r;
}
// length
inline float abs  (const vec4i& lhs) {return sqrt(imul16(lhs.x, lhs.x) + imul16(lhs.y, lhs.y) + imul16(lhs.z, lhs.z));}
// norm
inline vec4i norm(const vec4i& lhs) {vec4i r = lhs; return (r / abs(r));}



// type conversions
inline vec4f::operator vec4x() {vec4x r; fist(&r.x, x*65536.0f); fist(&r.y, y*65536.0f); fist(&r.z, z*65536.0f); fist(&r.w, w*65536.0f); return r;}
inline vec4x::operator vec4f() {vec4f r = {((float)x / 65536.0f), ((float)y / 65536.0f), ((float)z / 65536.0f), ((float)w / 65536.0f)}; return r;}



#endif

