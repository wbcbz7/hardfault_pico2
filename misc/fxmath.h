#pragma once

#include <stdlib.h>
#include <math.h>

#ifndef min
#define min(a, b)      ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b)      ((a) > (b) ? (a) : (b))
#endif

#define sgn(a)         ((a) < (0) ? (-1) : ((a) > (0) ? (1) : (0)))
#define clamp(a, l, h) ((a) > (h) ? (h) : ((a) < (l) ? (l) : (a)))

#define ee 10E-8
#define sqr(a) ((a)*(a))
#define pi 3.141592653589793f

// upside-down implementation of smoothstep()
inline float smoothstep(float edge0, float edge1, float x) {
  return (edge0 + (x * x * (3 - 2 * x)) * (edge1 - edge0));
}

/*
long abs(long a);
#pragma aux abs = \
    "mov    edx, eax" \
    "sar    edx, 31"  \
    "xor    eax, edx" \
    "sub    eax, edx" \
    parm [eax] value [eax] modify [eax edx]
*/
// fatmap2 ripoff :]

inline long ceilx(long a) {return (a + 0xFFFF) >> 16;}
inline long ceilx16(long a) {return (a + 0xFFFF) & ~0xFFFF;}
inline long sfract16(long a) {return (a - (a & ~0xFFFF));}       // signed fract16()

inline long imul16(long x, long y) {return (long)(((int64_t)x * y) >> 16);}        // (x * y) >> 16

inline long imul8(long x, long y) {return (long)(((int64_t)x * y) >> 8);}          // (x * y) >> 8

inline long imul14(long x, long y) {return (long)(((int64_t)x * y) >> 14);}        // (x * y) >> 14

inline long idiv16(long x, long y) {return (long)(((int64_t)x << 16) / y);}        // (x << 16) / y

inline long idiv8(long x, long y) {return (long)(((int64_t)x << 8) / y);}          // (x << 8) / y

inline long imuldiv(long x, long y, long z) {return (long)(((int64_t)x * y) / z);} // (x * y) / z, 64 bit precision

// *dst = (long) src;
inline void fist(long * dst, double src) { *dst = (long)src; }

inline long fistf(double src) { return (long)(src); }
    
inline long fistfx(double src) { return (long)(src * 65536.0f); }
    
inline long fistfx8(double src) { return (long)(src * 65536.0f * 256.0f); }

inline long fistfxtex(double src) { return (long)(src * 256.0f); }
