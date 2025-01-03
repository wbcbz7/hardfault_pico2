#include <math.h>
#include "fxmath.h"
#include "palerp.h"

void pal_lerp(argb32 *dst, argb32 *col0, argb32 *col1, int colors, int steps) {
    for (int i = 0; i < steps+1; i++) {
        argb32 *c0 = col0;
        argb32 *c1 = col1;
        for (int c = 0; c < colors; c++) {
            int r = ((c0->r * steps) + ((((c1->r) - (c0->r)) * i))); 
            int g = ((c0->g * steps) + ((((c1->g) - (c0->g)) * i))); 
            int b = ((c0->b * steps) + ((((c1->b) - (c0->b)) * i))); 
            dst->r = r / steps;
            dst->g = g / steps;
            dst->b = b / steps;
            dst++;
            c0++; c1++;
        }
    }
}

void pal_lerp_single_rgb555_256(uint16_t *dst, argb32 col0, argb32 col1) {
    argb32 c0 = col0;
    argb32 c1 = col1;
    for (int c = 0; c < 256; c++) {
        int r = (((c0.r) << 8) + ((((int)(c1.r) - (int)(c0.r)) * c))); 
        int g = (((c0.g) << 8) + ((((int)(c1.g) - (int)(c0.g)) * c))); 
        int b = (((c0.b) << 8) + ((((int)(c1.b) - (int)(c0.b)) * c))); 
        r = (r >> (8+3)) & 0xFF;
        g = (g >> (8+3)) & 0xFF;
        b = (b >> (8+3)) & 0xFF;
        *dst++ = ((r << 10) | (g << 5) | (b << 0));
    }
}

#if 0
// calcualte color-graded palette
void pal_calc(ptc_palette *dst, argb32 c0, argb32 c1, float *cpow, int start, int steps, bool saturate_white) {
    float cf0[3], cf1[3];
    cf0[0] = c0.r/255.0; cf0[1] = c0.g/255.0; cf0[2] = c0.b/255.0;
    cf1[0] = c1.r/255.0; cf1[1] = c1.g/255.0; cf1[2] = c1.b/255.0;

    ptc_argb32 *d = dst->data + start;
    float t = 0;
    for (int i = 0; i < steps; i++, t += (1.0 / steps)) {
        float out[3];
        out[0] = pow(cf0[0] + t * (cf1[0] - cf0[0]), cpow[0]);
        out[1] = pow(cf0[1] + t * (cf1[1] - cf0[1]), cpow[1]);
        out[2] = pow(cf0[2] + t * (cf1[2] - cf0[2]), cpow[2]);

        // cce saturate
        float maxcomp = max(0.3*out[0], max(0.5*out[1], 0.2*out[2]));
        maxcomp = 0.2*maxcomp*maxcomp;
        out[0] = out[0] + maxcomp;
        out[1] = out[1] + maxcomp;
        out[2] = out[2] + maxcomp;

        // tonemap result
        d->r = min(out[0], 1.0) * 255;
        d->g = min(out[1], 1.0) * 255;
        d->b = min(out[2], 1.0) * 255;
        d++;
    }
}

void pal_fade(ptc_palette *dst, ptc_palette *src, argb32 col, int32_t step, int start, int length) {
    int r = (col.r * (255 - step));
    int g = (col.g * (255 - step));
    int b = (col.b * (255 - step));

    ptc_argb32 *d = dst->data + start;
    ptc_argb32 *s = src->data + start;
    for (int i = 0; i < length; i++) {
        d->r = ((s->r * step) + r) >> 8;
        d->g = ((s->g * step) + g) >> 8;
        d->b = ((s->b * step) + b) >> 8;
        s++; d++;
    }
}
#endif

