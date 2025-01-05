#include <stdio.h>
#include <stdint.h>
#include "fbstuff.h"
#include "../defs.h"

// fill frame buffer with constant color
void __scratch_x("") fb_fill(void *dst, uint32_t color, uint32_t length) {
    color = (color & 0x7FFF);
    color |= (color << 16);
    length >>= 1; if (length == 0) return;
    uint32_t *p = (uint32_t*)dst;
    do {
        *p++ = color;
    } while(--length);
}

// blend dst buffer and constant
void __scratch_x("") fb_blend_const(void *dst, uint32_t color, uint32_t length) {
    color = (color & 0x7FFF);
    color |= (color << 16);
    color = color & 0x7BDE7BDE;
    length >>= 1; if (length == 0) return;
    uint32_t *p = (uint32_t*)dst;
    uint32_t blendmask = 0x7BDE7BDE;
    do {
        *p = (((*p & blendmask) + color) >> 1); p++;
    } while(--length);
}

// blend src and dst buffer
void __scratch_x("") fb_blend_buf(void *dst, void *src, uint32_t length) {
    length >>= 1; if (length == 0) return;
    uint32_t *p = (uint32_t*)dst;
    uint32_t *v = (uint32_t*)src;
    uint32_t blendmask = 0x7BDE7BDE;
    do {
        *p = ((*p & blendmask) + (*v & blendmask)) >> 1; p++; v++;
    } while(--length);
}
