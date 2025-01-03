#pragma once
#include <stdint.h>
#include <vec.h>

// returns 0 if fully in, -1 if fully out (rejected), else bit 0 - a clipped, bit 1 - b clipped, clips in place!
int lineclip(vec2f *a, vec2f *b, vec4f *bbox);

// not a bresenham :]
void drawline_subpixel(uint16_t *dst, int x1, int y1, int x2, int y2, int color);

