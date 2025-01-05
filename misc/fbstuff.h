#pragma once
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// fill frame buffer with constant color
void fb_fill(void *dst, uint32_t color, uint32_t length);
void fb_fill_a(void *dst, uint32_t color, uint32_t length);

// blend dst buffer and constant
void fb_blend_const(void *dst, uint32_t color, uint32_t length);
void fb_blend_const_a(void *dst, uint32_t color, uint32_t length);

// blend src and dst buffer
void fb_blend_buf(void *dst, void *src, uint32_t length);
void fb_blend_buf_a(void *dst, void *src, uint32_t length);

#ifdef __cplusplus
}
#endif
