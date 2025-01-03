#pragma once
#include <stdint.h>
#include "argb.h"

// generate palette mix (note: generates steps+1 sets, make sure you're not screwing memory!)
void pal_lerp(argb32 *dst, argb32 *col0, argb32 *col1, int colors, int steps);

void pal_lerp_single_rgb555_256(uint16_t *dst, argb32 col0, argb32 col1);

#if 0
// mix between constant color and palette
void pal_fade(ptc_palette *dst, ptc_palette *src, argb32 col, int32_t step, int start, int length);

void pal_calc(ptc_palette *dst, argb32 c0, argb32 c1, float *cpow, int start, int steps, bool saturate_white);
#endif