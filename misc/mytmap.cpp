#include <stdint.h>
#include <stdio.h>
#include <defs.h>
#include "vec.h"
#include "fxmath.h"
#include "mytmap.h"
#include "hardware/interp.h"

#define USE_INTERP

extern "C" {

inline int next3(int i) {return (1 << i) & 3;}  
inline int next4(int i) {return (i + 1) & 3;}         // that's VERY simple indeed

// ---------------------------

/*
    conventions:
    everything is subpixel (and optionally subtextel) corrected but this can be tweaked if needed
    
    poly vertex work layout:
    x = x, y = y, z = d/z (d is any value > 0), w = ceil(y)

    ах да, переписываем половину кода нахуй
    конкретно - всё вычисляем во float и переходим в fixedpoint на этапе интерполяции по ребрам
    так быстрее (в теории :), меньше риск переполнений и проще впиливать коррекцию перспективы
*/

static uint8_t  *mytmap_dst;
static uint32_t  mytmap_pitch;

#define MAX_EDGES 32
#define MAX_VERTICES 32
#define MAX_SECTIONS (MAX_EDGES - 1)

#define MAX_SPAN_WIDTH_LOG 5
#define MAX_SPAN_WIDTH     (1 << MAX_SPAN_WIDTH_LOG)

#define TMAP_EDGE_INTFLOAT(a) union {int32_t a; float f##a;};

#define SUBPIXEL_TABLE
#define SUBPIXEL_BITS 2
#define TMAP_PRESTEP_INTFLOAT(a) union {int32_t a[1 << SUBPIXEL_BITS]; float f##a[1 << SUBPIXEL_BITS];};

enum {
    SIDE_LEFT  = 0,
    SIDE_RIGHT = 1,
};

struct tmap_edge_lerp_t {
    int32_t side;       // apply to left or right side
    int32_t height;     // edge height
    int32_t x;          // always fixed point
    int32_t dxdy;       // always fixed point

    TMAP_EDGE_INTFLOAT(u);          // u or u/z
    TMAP_EDGE_INTFLOAT(dudy);
    TMAP_EDGE_INTFLOAT(v);          // v or v/z
    TMAP_EDGE_INTFLOAT(dvdy);
    TMAP_EDGE_INTFLOAT(l);          // l or l/z
    TMAP_EDGE_INTFLOAT(dldy);
    TMAP_EDGE_INTFLOAT(u2);         // same as u for dual texture
    TMAP_EDGE_INTFLOAT(du2dy);
    TMAP_EDGE_INTFLOAT(v2);         // --//--  v  -------//------
    TMAP_EDGE_INTFLOAT(dv2dy);
};

// polygon constant gradients
struct poly_lerp_gradients_t {
    uint32_t color;
    union {uint8_t  *texture;  uint16_t *texture16;};
    union {uint8_t  *texture2; uint16_t *texture2_16;};
    union {
        uint8_t  *blendtab;     // 256x256
        uint16_t *blendtab16;     // 256x256
    };
    uint32_t uvmask;
    uint32_t uvmask2;

    TMAP_EDGE_INTFLOAT(dudx);
    TMAP_EDGE_INTFLOAT(dvdx);
    TMAP_EDGE_INTFLOAT(dldx);
    TMAP_EDGE_INTFLOAT(du2dx);
    TMAP_EDGE_INTFLOAT(dv2dx);

    TMAP_PRESTEP_INTFLOAT(dudx_prestep);
    TMAP_PRESTEP_INTFLOAT(dvdx_prestep);
    TMAP_PRESTEP_INTFLOAT(dldx_prestep);
    TMAP_PRESTEP_INTFLOAT(du2dx_prestep);
    TMAP_PRESTEP_INTFLOAT(dv2dx_prestep);
};

static poly_lerp_gradients_t grad;

// texture coordinates buffer for perspective correct mapper
static vec2f tmap_texcoord [MAX_VERTICES];
static vec2f tmap_texcoord2[MAX_VERTICES];

// edge interpolation buffer (not the edge buffer itself :)
static tmap_edge_lerp_t edge_lerp[MAX_EDGES];

// polygon section heights
static int32_t section_heights[MAX_SECTIONS+1];   // leave place for null section

// ------------------------------------
// init
void mytmap_polydraw_init(void *buf, int pitch) {
    mytmap_dst = (uint8_t*)buf;
    mytmap_pitch = pitch;
}

// ------------------------------------
// setup interpolators for 16.16 fixedpoint operation
// bit_bias shifts result to the left (for halfword/word tables)
void mytmap_interp_setup_l(interp_hw_t *interp, void *texture, uint32_t fract_bits, uint32_t width_bits, uint32_t height_bits, uint32_t bit_bias) {
    interp_config cfg = interp_default_config();
    interp_config_set_add_raw(&cfg, true);

    // setup lane 0
    interp_config_set_shift(&cfg, fract_bits - bit_bias);
    interp_config_set_mask(&cfg, bit_bias, bit_bias + width_bits - 1);
    interp_set_config(interp, 0, &cfg);

    // setup lane 1
    interp->base[1] = interp->accum[1] = 0;    // lane 1 is nop

    // setup lane 2
    interp->base[2] = (uintptr_t) texture;
}

void mytmap_interp_setup_l_2x2(interp_hw_t *interp, void *texture, uint32_t fract_bits, uint32_t width_bits, uint32_t height_bits, uint32_t bit_bias) {
    interp_config cfg = interp_default_config();
    interp_config_set_add_raw(&cfg, true);

    // setup lane 0
    interp_config_set_shift(&cfg, (fract_bits - bit_bias) & 31);
    interp_config_set_mask(&cfg, bit_bias, bit_bias + width_bits - 1);
    interp_set_config(interp, 0, &cfg);

    // setup lane 1 (the dithering lane)
    interp_config_set_shift(&cfg, (-bit_bias-1) & 31);
    interp_config_set_mask(&cfg, bit_bias+1, bit_bias + 1 + 2 - 1);
    interp_set_config(interp, 1, &cfg);
    interp->accum[1] = interp->base[1] = 2;

    // setup lane 2
    interp->base[2] = (uintptr_t) texture;
}

void mytmap_interp_setup_uv(interp_hw_t *interp, void *texture, uint32_t fract_bits, uint32_t width_bits, uint32_t height_bits, uint32_t bit_bias) {
    interp_config cfg = interp_default_config();
    interp_config_set_add_raw(&cfg, true);

    // setup lane 0
    interp_config_set_shift(&cfg, fract_bits - bit_bias);
    interp_config_set_mask(&cfg, bit_bias, bit_bias + width_bits - 1);
    interp_set_config(interp, 0, &cfg);

    // setup lane 1
    interp_config_set_shift(&cfg, fract_bits - width_bits - bit_bias);
    interp_config_set_mask(&cfg, bit_bias + width_bits, bit_bias + width_bits + height_bits - 1);
    interp_set_config(interp, 1, &cfg);

    // setup lane 2
    interp->base[2] = (uintptr_t) texture;
}

// ------------------------------------
// add edge to the buffer, returns edge height
static int mytmap_add_edge_flat_ffx(tmap_edge_lerp_t *edge, int32_t side, tmap_vtx_uv_t *v1, tmap_vtx_uv_t *v2) {
    vec3f  *p1 = v1->fp,  *p2 = v2->fp;
    int32_t height     = edge->height = v2->cy - v1->cy;
    float   inv_height;
    float   dy;
    
    edge->side = side;
    
    if (height > 0) {
        float dx = p2->x - p1->x;
              dy = p2->y - p1->y;
        inv_height = 65536.0f / dy;
        edge->dxdy = (dx * inv_height);

        // prestep
        float prestep = (ceil(p1->y) - p1->y);
        edge->x = fistfx(p1->x) + (edge->dxdy * prestep);
    }
    return height;
}

static void mytmap_draw_sections_affine_flat(uint8_t *dst, int sections, int32_t *heights, tmap_edge_lerp_t *edges) {
    // common tmap code here
    tmap_edge_lerp_t *left = edge_lerp + 0, *right = edge_lerp + 1, *next_edge = edge_lerp + 2;
    uint32_t color = grad.color;

    // draw polygon sections
    do {
        int lines = *heights++;
        if (lines > 0) do {
            int32_t start = ceilx(left->x);
            int32_t width = ceilx(right->x) - start;
            uint8_t *p = dst + start;
            if (width > 0) do {
                *p++ = color;
            } while (--width);
            left->x  += left->dxdy;
            right->x += right->dxdy;
            dst += mytmap_pitch;
        } while (--lines);

        // switch to next section
        next_edge->side ? right = next_edge : left = next_edge;
        next_edge++;
    } while (--sections);
}

static void mytmap_draw_sections_affine_flat_16(uint8_t *dst, int sections, int32_t *heights, tmap_edge_lerp_t *edges) {
    // common tmap code here
    tmap_edge_lerp_t *left = edge_lerp + 0, *right = edge_lerp + 1, *next_edge = edge_lerp + 2;
    uint32_t color = grad.color;

    // draw polygon sections
    do {
        int lines = *heights++;
        if (lines > 0) do {
            int32_t start = ceilx(left->x);
            int32_t width = ceilx(right->x) - start;
            uint16_t *p = (uint16_t*)dst + start;
            if (width > 0) do {
                *p++ = color;
            } while (--width);
            left->x  += left->dxdy;
            right->x += right->dxdy;
            dst += mytmap_pitch;
        } while (--lines);

        // switch to next section
        next_edge->side ? right = next_edge : left = next_edge;
        next_edge++;
    } while (--sections);
}

// -------------------------------
// lighting, float
// add edge to the buffer, returns edge height
static int mytmap_add_edge_l_ffx(tmap_edge_lerp_t *edge, int32_t side, tmap_vtx_uv_t *v1, tmap_vtx_uv_t *v2) {
    vec3f  *p1 = v1->fp, *p2 = v2->fp;
    float   l1 = v1->fl,  l2 = v2->fl;
    int32_t height     = edge->height = v2->cy - v1->cy;
    float   inv_height;
    float   dy;
    
    edge->side = side;
    
    if (height > 0) {
        float dx = p2->x - p1->x;
              dy = p2->y - p1->y;
        inv_height = 65536.0f / dy;
        edge->dxdy = (dx * inv_height);

        // prestep
        float prestep = (ceil(p1->y) - p1->y);
        edge->x = (p1->x * 65536.0f) + (edge->dxdy * prestep);
        
        if (edge->side == SIDE_LEFT) {
            float dl = l2 - l1;

            edge->dldy = (dl * inv_height); 
            edge->l = (l1 * 65536.0f) + (edge->dldy * prestep);
        }
    }
    return height;
}

// -------------------------------
// affine, float
// add edge to the buffer, returns edge height
static int mytmap_add_edge_uv_ffx(tmap_edge_lerp_t *edge, int32_t side, tmap_vtx_uv_t *v1, tmap_vtx_uv_t *v2) {
    vec3f  *p1 = v1->fp,  *p2 = v2->fp;
    vec2f  *t1 = v1->fuv, *t2 = v2->fuv;
    int32_t height     = edge->height = v2->cy - v1->cy;
    float   inv_height;
    float   dy;
    
    edge->side = side;
    
    if (height > 0) {
        float dx = p2->x - p1->x;
              dy = p2->y - p1->y;
        inv_height = 65536.0f / dy;
        edge->dxdy = (dx * inv_height);

        // prestep
        float prestep = (ceil(p1->y) - p1->y);
        edge->x = (p1->x * 65536.0f) + (edge->dxdy * prestep);
        
        if (edge->side == SIDE_LEFT) {
            float du = t2->x - t1->x;
            float dv = t2->y - t1->y;

            edge->dudy = (du * inv_height); 
            edge->dvdy = (dv * inv_height); 
            edge->u = (t1->x * 65536.0f) + (edge->dudy * prestep);
            edge->v = (t1->y * 65536.0f) + (edge->dvdy * prestep);
        }
    }
    return height;
}

#ifdef USE_INTERP
#if 1
static void __not_in_flash_func(mytmap_draw_sections_affine_l_16)(uint8_t *dst, int sections, int start_y, int32_t *heights, tmap_edge_lerp_t *edges) {
    // common tmap code here
    tmap_edge_lerp_t *left = edge_lerp + 0, *right = edge_lerp + 1, *next_edge = edge_lerp + 2;

    int ditherbase = start_y & 1 ? 3 : 0;
    // draw polygon sections
    do {
        int lines = *heights++;
        if (lines > 0) do {
            int32_t start   = ceilx(left->x);
            int32_t width   = ceilx(right->x) - start;
            int32_t prestep = (-left->x)&0xFFFF;
#ifdef SUBPIXEL_TABLE
            int32_t l = left->l + grad.dldx_prestep[prestep >> (16 - SUBPIXEL_BITS)];
#else
            int32_t l = left->l + imul16(prestep, grad.dldx);
#endif
            MYTMAP_INTERP_SHADETAB->accum[0] = l;
            MYTMAP_INTERP_SHADETAB->accum[1] = ditherbase;
            uint16_t *p = (uint16_t*)dst + start;
            if (width > 0) do {
                *p++ = *(uint16_t*)MYTMAP_INTERP_SHADETAB->pop[2];
            } while (--width);
            left->x  += left->dxdy;
            right->x += right->dxdy;
            left->l  += left->dldy;
            dst += mytmap_pitch;
            ditherbase ^= 3;
        } while (--lines);

        // switch to next section
        next_edge->side ? right = next_edge : left = next_edge;
        next_edge++;
    } while (--sections);
}
#else
static void __not_in_flash_func(mytmap_draw_sections_affine_l_16)(uint8_t *dst, int sections, int start_y, int32_t *heights, tmap_edge_lerp_t *edges) {
    // common tmap code here
    tmap_edge_lerp_t *left = edge_lerp + 0, *right = edge_lerp + 1, *next_edge = edge_lerp + 2;

    // draw polygon sections
    do {
        int lines = *heights++;
        if (lines > 0) do {
            int32_t start   = ceilx(left->x);
            int32_t width   = ceilx(right->x) - start;
            int32_t prestep = (-left->x)&0xFFFF;
#ifdef SUBPIXEL_TABLE
            int32_t l = left->l + grad.dldx_prestep[prestep >> (16 - SUBPIXEL_BITS)];
#else
            int32_t l = left->l + imul16(prestep, grad.dldx);
#endif
            MYTMAP_INTERP_SHADETAB->accum[0] = l;
            uint16_t *p = (uint16_t*)dst + start;
            if (width > 0) do {
                *p++ = *(uint16_t*)MYTMAP_INTERP_SHADETAB->pop[2];
            } while (--width);
            left->x  += left->dxdy;
            right->x += right->dxdy;
            left->l  += left->dldy;
            dst += mytmap_pitch;
        } while (--lines);

        // switch to next section
        next_edge->side ? right = next_edge : left = next_edge;
        next_edge++;
    } while (--sections);
}
#endif
#else
#if 1
static void __not_in_flash_func(mytmap_draw_sections_affine_l_16)(uint8_t *dst, int sections, int start_y, int32_t *heights, tmap_edge_lerp_t *edges) {
    // common tmap code here
    tmap_edge_lerp_t *left = edge_lerp + 0, *right = edge_lerp + 1, *next_edge = edge_lerp + 2;

    // draw polygon sections
    do {
        int lines = *heights++;
        if (lines > 0) do {
            int32_t start   = ceilx(left->x);
            int32_t width   = ceilx(right->x) - start;
            int32_t prestep = (-left->x)&0xFFFF;
            int32_t l = left->l + imul16(prestep, grad.dldx);
            uint16_t *p = (uint16_t*)dst + start;
            if (width > 0) do {
                *p++ = grad.texture16[(l>>16) & grad.uvmask];
                l += grad.dldx;
            } while (--width);
            left->x  += left->dxdy;
            right->x += right->dxdy;
            left->l  += left->dldy;
            dst += mytmap_pitch;
        } while (--lines);

        // switch to next section
        next_edge->side ? right = next_edge : left = next_edge;
        next_edge++;
    } while (--sections);
}
#else
// with 2x2 dithering
static void __not_in_flash_func(mytmap_draw_sections_affine_l_16)(uint8_t *dst, int sections, int start_y, int32_t *heights, tmap_edge_lerp_t *edges) {
    // common tmap code here
    tmap_edge_lerp_t *left = edge_lerp + 0, *right = edge_lerp + 1, *next_edge = edge_lerp + 2;

    // draw polygon sections
    int32_t dither_y = ((start_y&1) ? 3 : 0) << (16+1);
    do {
        int lines = *heights++;
        if (lines > 0) do {
            int32_t start   = ceilx(left->x);
            int32_t width   = ceilx(right->x) - start;
            int32_t prestep = (-left->x)&0xFFFF;
            int32_t l = left->l + imul16(prestep, grad.dldx);
            int32_t d = dither_y ^ ((start&1) << (16+2));
            uint16_t *p = (uint16_t*)dst + start;
            if (width > 0) do {
                *p++ = grad.texture16[((l + d)>>16) & grad.uvmask];
                l += grad.dldx;
                d ^= 2 << (16+1);
            } while (--width);
            left->x  += left->dxdy;
            right->x += right->dxdy;
            left->l  += left->dldy;
            dither_y ^= 3 << (16+1);
            dst += mytmap_pitch;
        } while (--lines);

        // switch to next section
        next_edge->side ? right = next_edge : left = next_edge;
        next_edge++;
    } while (--sections);
}
#endif
#endif

static void mytmap_draw_sections_affine_uv_16(uint8_t *dst, int sections, int32_t *heights, tmap_edge_lerp_t *edges) {
    // common tmap code here
    tmap_edge_lerp_t *left = edge_lerp + 0, *right = edge_lerp + 1, *next_edge = edge_lerp + 2;

    // draw polygon sections
    do {
        int lines = *heights++;
        if (lines > 0) do {
            int32_t start   = ceilx(left->x);
            int32_t width   = ceilx(right->x) - start;
            int32_t prestep = (-left->x)&0xFFFF;
            int32_t u = left->u + imul16(prestep, grad.dudx);
            int32_t v = left->v + imul16(prestep, grad.dvdx);
            uint16_t *p = (uint16_t*)dst + start;
            if (width > 0) do {
                *p++ = grad.texture16[(((u>>16)&0xFF) | ((v>>8)&0xFF00)) & grad.uvmask];
                u += grad.dudx;
                v += grad.dvdx;
            } while (--width);
            left->x  += left->dxdy;
            right->x += right->dxdy;
            left->u  += left->dudy;
            left->v  += left->dvdy;
            dst += mytmap_pitch;
        } while (--lines);

        // switch to next section
        next_edge->side ? right = next_edge : left = next_edge;
        next_edge++;
    } while (--sections);
}

// -------------------------------
// affine + multitex, float
// add edge to the buffer, returns edge height
static int mytmap_add_edge_multiuv_ffx(tmap_edge_lerp_t *edge, int32_t side, tmap_vtx_uv_t *v1, tmap_vtx_uv_t *v2) {
    vec3f  *p1 = v1->fp,   *p2 = v2->fp;
    vec2f  *t1 = v1->fuv,  *t2 = v2->fuv;
    vec2f  *m1 = v1->fuv2, *m2 = v2->fuv2;
    int32_t height     = edge->height = v2->cy - v1->cy;
    float   inv_height;
    float   dy;
    
    edge->side = side;
    
    if (height > 0) {
        float dx = p2->x - p1->x;
              dy = p2->y - p1->y;
        inv_height = 65536.0f / dy;
        edge->dxdy = (dx * inv_height);

        // prestep
        float prestep = (ceil(p1->y) - p1->y);
        edge->x = (p1->x * 65536.0f) + (edge->dxdy * prestep);
        
        if (edge->side == SIDE_LEFT) {
            float du  = t2->x - t1->x;
            float dv  = t2->y - t1->y;
            float du2 = m2->x - m1->x;
            float dv2 = m2->y - m1->y;

            edge->dudy = (du * inv_height); 
            edge->dvdy = (dv * inv_height); 
            edge->u = (t1->x * 65536.0f) + (edge->dudy * prestep);
            edge->v = (t1->y * 65536.0f) + (edge->dvdy * prestep);

            edge->du2dy = (du2 * inv_height); 
            edge->dv2dy = (dv2 * inv_height); 
            edge->u2 = (m1->x * 65536.0f) + (edge->du2dy * prestep);
            edge->v2 = (m1->y * 65536.0f) + (edge->dv2dy * prestep);
        }
    }
    return height;
}

static void mytmap_draw_sections_affine_multiuv_16(uint8_t *dst, int sections, int32_t *heights, tmap_edge_lerp_t *edges) {
    // common tmap code here
    tmap_edge_lerp_t *left = edge_lerp + 0, *right = edge_lerp + 1, *next_edge = edge_lerp + 2;

    // draw polygon sections
    do {
        int lines = *heights++;
        if (lines > 0) do {
            int32_t start   = ceilx(left->x);
            int32_t width   = ceilx(right->x) - start;
            int32_t prestep = (-left->x)&0xFFFF;
            int32_t u  = left->u  + imul16(prestep, grad.dudx);
            int32_t v  = left->v  + imul16(prestep, grad.dvdx);
            int32_t u2 = left->u2 + imul16(prestep, grad.du2dx);
            int32_t v2 = left->v2 + imul16(prestep, grad.dv2dx);
            uint16_t *p = (uint16_t*)dst + start;
            if (width > 0) do {
                uint32_t uv1 = (((u >>16)&0xFF) | ((v >>8)&0xFF00)) & grad.uvmask;
                uint32_t uv2 = (((u2>>16)&0xFF) | ((v2>>8)&0xFF00)) & grad.uvmask2;
                *p++ = grad.blendtab16[(grad.texture[uv1]<<0) | (grad.texture2[uv2]<<8)];
                u  += grad.dudx;
                v  += grad.dvdx;
                u2 += grad.du2dx;
                v2 += grad.dv2dx;
            } while (--width);
            left->x  += left->dxdy;
            right->x += right->dxdy;
            left->u  += left->dudy;
            left->v  += left->dvdy;
            left->u2 += left->du2dy;
            left->v2 += left->dv2dy;
            dst += mytmap_pitch;
        } while (--lines);

        // switch to next section
        next_edge->side ? right = next_edge : left = next_edge;
        next_edge++;
    } while (--sections);
}

// -------------------------------------------

// poly setup tables
static const struct tri_setup_t {
    uint8_t top, mid, bottom, left, right, side, pad[2]; 
} tri_setup_table[] = {
    {0},        // empty
    {2, 1, 0, 0, 1, SIDE_RIGHT},
    {1, 0, 2, 2, 0, SIDE_RIGHT},
    {1, 2, 0, 2, 0, SIDE_LEFT},
    {0, 2, 1, 1, 2, SIDE_RIGHT},
    {2, 0, 1, 0, 1, SIDE_LEFT},
    {0, 1, 2, 1, 2, SIDE_LEFT},
};


// -------------------------------------------

// polygon fillers

// using poly setup table
void mytmap_draw_tri_flat(tmap_vtx_uv_t *f, uint32_t color) {
    tmap_vtx_uv_t *top_vtx, *bottom_vtx, *mid_vtx, *left_vtx, *right_vtx;

    // calculate index, precompute ceil(y)
    // note type punning here - since Y for all vertices is positive this should be fine
    int index =            ((f[0].p->y < f[1].p->y) ? 1 : 0); f[0].cy = ceil(f[0].fp->y);
    index = (index << 1) | ((f[1].p->y < f[2].p->y) ? 1 : 0); f[1].cy = ceil(f[1].fp->y);
    index = (index << 1) | ((f[2].p->y < f[0].p->y) ? 1 : 0); f[2].cy = ceil(f[2].fp->y);
    if (index == 0) return; // degenerate case

    const tri_setup_t *ps = tri_setup_table + index;

    // get vertex pointers
    top_vtx     = f + ps->top;
    mid_vtx     = f + ps->mid;
    bottom_vtx  = f + ps->bottom;
    left_vtx    = f + ps->left;
    right_vtx   = f + ps->right;

    // compute edge sections
    mytmap_add_edge_flat_ffx(edge_lerp + 0, SIDE_LEFT,  top_vtx, left_vtx);
    mytmap_add_edge_flat_ffx(edge_lerp + 1, SIDE_RIGHT, top_vtx, right_vtx);
    mytmap_add_edge_flat_ffx(edge_lerp + 2, ps->side,   mid_vtx, bottom_vtx);
    
    // and section heights
    section_heights[0] = mid_vtx->cy    - top_vtx->cy;
    section_heights[1] = bottom_vtx->cy - mid_vtx->cy;

    // get display start
    uint8_t* dst_start = mytmap_dst + ((top_vtx->cy) * mytmap_pitch);

    // call common filler
    grad.color = color;
    mytmap_draw_sections_affine_flat_16(dst_start, 2, section_heights, edge_lerp);
}

// using poly setup table
void mytmap_draw_tri_flat_16(tmap_vtx_uv_t *f, uint32_t color) {
    tmap_vtx_uv_t *top_vtx, *bottom_vtx, *mid_vtx, *left_vtx, *right_vtx;

    // calculate index, precompute ceil(y)
    // note type punning here - since Y for all vertices is positive this should be fine
    int index =            ((f[0].p->y < f[1].p->y) ? 1 : 0); f[0].cy = ceil(f[0].fp->y);
    index = (index << 1) | ((f[1].p->y < f[2].p->y) ? 1 : 0); f[1].cy = ceil(f[1].fp->y);
    index = (index << 1) | ((f[2].p->y < f[0].p->y) ? 1 : 0); f[2].cy = ceil(f[2].fp->y);
    if (index == 0) return; // degenerate case

    const tri_setup_t *ps = tri_setup_table + index;

    // get vertex pointers
    top_vtx     = f + ps->top;
    mid_vtx     = f + ps->mid;
    bottom_vtx  = f + ps->bottom;
    left_vtx    = f + ps->left;
    right_vtx   = f + ps->right;

    // compute edge sections
    mytmap_add_edge_flat_ffx(edge_lerp + 0, SIDE_LEFT,  top_vtx, left_vtx);
    mytmap_add_edge_flat_ffx(edge_lerp + 1, SIDE_RIGHT, top_vtx, right_vtx);
    mytmap_add_edge_flat_ffx(edge_lerp + 2, ps->side,   mid_vtx, bottom_vtx);
    
    // and section heights
    section_heights[0] = mid_vtx->cy    - top_vtx->cy;
    section_heights[1] = bottom_vtx->cy - mid_vtx->cy;

    // get display start
    uint8_t* dst_start = mytmap_dst + ((top_vtx->cy) * mytmap_pitch);

    // call common filler
    grad.color = color;
    mytmap_draw_sections_affine_flat_16(dst_start, 2, section_heights, edge_lerp);
}

// using poly setup table
void mytmap_draw_poly_flat_16(tmap_vtx_uv_t *f, uint32_t vtxcount, uint32_t color) {
    tmap_vtx_uv_t *top_vtx, *bottom_vtx, *mid_vtx, *left_vtx, *right_vtx, *start_vtx, *end_vtx;

    start_vtx   = f;
    end_vtx     = f + vtxcount - 1;

    // find top and bottom vertices
    int top_idx = 0, bottom_idx = 0, mid_idx; int i = 1;
    f[0].cy = ceil(f[0].fp->y);
    do {
        f[i].cy = ceil(f[i].fp->y); // precalc ceil(y)
        if (f[i].fp->y < f[top_idx].fp->y) {
            top_idx = i; 
        } else if (f[i].fp->y > f[bottom_idx].fp->y) {
            bottom_idx = i;
        }
    } while (++i != vtxcount);
    if (top_idx == bottom_idx) return;  // nothing to draw!

    // find middle vertex
    mid_idx = top_idx + 1; if (mid_idx >= vtxcount) mid_idx = 0;
    if (mid_idx == bottom_idx) {
        mid_idx = bottom_idx + 1; 
        if (mid_idx >= vtxcount) mid_idx = 0;
    }
    
    top_vtx     = f + top_idx;
    bottom_vtx  = f + bottom_idx;
    mid_vtx     = f + mid_idx;
    left_vtx    = f + top_idx + 1; if (left_vtx  > end_vtx)   left_vtx   = start_vtx;
    right_vtx   = f + top_idx - 1; if (right_vtx < start_vtx) right_vtx  = end_vtx;

    // add first two edges
    mytmap_add_edge_flat_ffx(edge_lerp + 0, SIDE_LEFT,  top_vtx, left_vtx);
    mytmap_add_edge_flat_ffx(edge_lerp + 1, SIDE_RIGHT, top_vtx, right_vtx);

    // iterate along left and right edges, calculate section heights
    int section_idx = 0;
    tmap_edge_lerp_t *edge_left = edge_lerp + 0, *edge_right = edge_lerp + 1, *edge_cur = edge_lerp + 2;
    int left_height = edge_left->height, right_height = edge_right->height;
    while (((left_vtx != bottom_vtx) || (right_vtx != bottom_vtx)) && (section_idx < vtxcount)) {
        if (left_height < right_height) {
            // next edge is left, adjust left pointer
            section_heights[section_idx++] = left_height;
            right_height -= left_height;
            // add new edge
            tmap_vtx_uv_t *prev_vtx = left_vtx;
            if (left_vtx != bottom_vtx) {
                left_vtx++; if (left_vtx > end_vtx) left_vtx = start_vtx;
                mytmap_add_edge_flat_ffx(edge_cur, SIDE_LEFT, prev_vtx, left_vtx);
                edge_left = edge_cur++;
                left_height = edge_left->height;
            }
        } else {
            // next edge is right, adjust right pointer
            section_heights[section_idx++] = right_height;
            left_height -= right_height;
            // add new edge
            tmap_vtx_uv_t *prev_vtx = right_vtx;
            if (right_vtx != bottom_vtx) {
                right_vtx--; if (right_vtx < start_vtx) right_vtx = end_vtx;
                mytmap_add_edge_flat_ffx(edge_cur, SIDE_RIGHT, prev_vtx, right_vtx);
                edge_right = edge_cur++;
                right_height = edge_right->height;
            }
        }
    }
    // set last section height
    section_heights[section_idx++] = (left_height < right_height) ? left_height : right_height;

    // get display start
    uint8_t* dst_start = mytmap_dst + ((top_vtx->cy) * mytmap_pitch);

    // call common filler
    grad.color = color;
    mytmap_draw_sections_affine_flat_16(dst_start, section_idx, section_heights, edge_lerp);
}

// textured, 8bpp
void mytmap_draw_tri_tex(tmap_vtx_uv_t *f, uint8_t *tex) {
    // setup table
    static const struct poly_setup_t {
        uint8_t top, mid, bottom, left, right, side, pad[2]; 
    } setup_table[] = {
        {0},        // empty
        {2, 1, 0, 0, 1, SIDE_RIGHT},
        {1, 0, 2, 2, 0, SIDE_RIGHT},
        {1, 2, 0, 2, 0, SIDE_LEFT},
        {0, 2, 1, 1, 2, SIDE_RIGHT},
        {2, 0, 1, 0, 1, SIDE_LEFT},
        {0, 1, 2, 1, 2, SIDE_LEFT},
    };

    tmap_vtx_uv_t *top_vtx, *bottom_vtx, *mid_vtx, *left_vtx, *right_vtx;

    // calculate index, precompute ceil(y)
    // note type punning here - since Y for all vertices is positive this should be fine
    int index =            ((f[0].p->y < f[1].p->y) ? 1 : 0); f[0].cy = ceil(f[0].fp->y);
    index = (index << 1) | ((f[1].p->y < f[2].p->y) ? 1 : 0); f[1].cy = ceil(f[1].fp->y);
    index = (index << 1) | ((f[2].p->y < f[0].p->y) ? 1 : 0); f[2].cy = ceil(f[2].fp->y);
    if (index == 0) return; // degenerate case

    const poly_setup_t *ps = setup_table + index;

    // get vertex pointers
    top_vtx     = f + ps->top;
    mid_vtx     = f + ps->mid;
    bottom_vtx  = f + ps->bottom;
    left_vtx    = f + ps->left;
    right_vtx   = f + ps->right;

    // calculate gradients
    vec3f *pt = top_vtx->fp, *pm = mid_vtx->fp, *pb = bottom_vtx->fp;
    float inv_height = 1.0f / (pb->y - pt->y);
    float mid_t = (pm->y - pt->y) * inv_height;
    float longest_width = ((pb->x - pt->x) * mid_t) + (pt->x - pm->x);
    if (longest_width == 0.0f) return;     // nothing to draw!
    float inv_width     = 1.0f / longest_width;
    float inv_width_64k = 65536.0f * inv_width;

    vec2f *tt = top_vtx->fuv, *tm = mid_vtx->fuv, *tb = bottom_vtx->fuv;

    // prepare texture coordinates
    int32_t scale_u = 256;
    int32_t scale_v = 256;
    for (int vtx = 0; vtx < 3; vtx++) {
        tmap_texcoord[vtx].x = (f[vtx].fuv->x * scale_u);
        tmap_texcoord[vtx].y = (f[vtx].fuv->y * scale_v);
        f[vtx].fuv = tmap_texcoord + vtx;
    }

    // calculate gradients
    grad.dudx = (int32_t)((((tb->x - tt->x) * mid_t) + (tt->x - tm->x)) * inv_width_64k * scale_u);
    grad.dvdx = (int32_t)((((tb->y - tt->y) * mid_t) + (tt->y - tm->y)) * inv_width_64k * scale_v);

    // compute edge sections
    mytmap_add_edge_uv_ffx(edge_lerp + 0, SIDE_LEFT,  top_vtx, left_vtx);
    mytmap_add_edge_uv_ffx(edge_lerp + 1, SIDE_RIGHT, top_vtx, right_vtx);
    mytmap_add_edge_uv_ffx(edge_lerp + 2, ps->side,   mid_vtx, bottom_vtx);
    
    // and section heights
    section_heights[0] = mid_vtx->cy    - top_vtx->cy;
    section_heights[1] = bottom_vtx->cy - mid_vtx->cy;

    // get display start
    uint8_t* dst_start = mytmap_dst + ((top_vtx->cy) * mytmap_pitch);

    // call common filler
    grad.texture = tex;
    grad.uvmask  = 0xFFFF;
    mytmap_draw_sections_affine_uv_16(dst_start, 2, section_heights, edge_lerp);
}

// gouraud via shade table, 16bpp
void mytmap_draw_tri_gouraud_16(tmap_vtx_uv_t *f, uint16_t *shadetab) {
    tmap_vtx_uv_t *top_vtx, *bottom_vtx, *mid_vtx, *left_vtx, *right_vtx;

    // calculate index, precompute ceil(y)
    // note type punning here - since Y for all vertices is positive this should be fine
    int index =            ((f[0].p->y < f[1].p->y) ? 1 : 0); f[0].cy = ceil(f[0].fp->y);
    index = (index << 1) | ((f[1].p->y < f[2].p->y) ? 1 : 0); f[1].cy = ceil(f[1].fp->y);
    index = (index << 1) | ((f[2].p->y < f[0].p->y) ? 1 : 0); f[2].cy = ceil(f[2].fp->y);
    if (index == 0) return; // degenerate case

    const tri_setup_t *ps = tri_setup_table + index;

    // get vertex pointers
    top_vtx     = f + ps->top;
    mid_vtx     = f + ps->mid;
    bottom_vtx  = f + ps->bottom;
    left_vtx    = f + ps->left;
    right_vtx   = f + ps->right;

    // calculate gradients
    vec3f *pt = top_vtx->fp, *pm = mid_vtx->fp, *pb = bottom_vtx->fp;
    float inv_height = 1.0f / (pb->y - pt->y);
    float mid_t = (pm->y - pt->y) * inv_height;
    float longest_width = ((pb->x - pt->x) * mid_t) + (pt->x - pm->x);
    if (longest_width == 0.0f) return;     // nothing to draw!
    float inv_width     = 1.0f / longest_width;
    float inv_width_64k = 65536.0f * inv_width;

    float lt = top_vtx->fl, lm = mid_vtx->fl, lb = bottom_vtx->fl;

    // calculate gradients
    grad.dldx  = (int32_t)((((lb - lt) * mid_t) + (lt - lm)) * inv_width_64k);
#ifdef SUBPIXEL_TABLE
    {
        int32_t l = 0;
        int32_t dl = (grad.dldx >> SUBPIXEL_BITS);
        for (int i = 0; i < (1 << SUBPIXEL_BITS); i++) {
            grad.dldx_prestep[i] = l; l += dl;
        }
    }
#endif

    // compute edge sections
    mytmap_add_edge_l_ffx(edge_lerp + 0, SIDE_LEFT,  top_vtx, left_vtx);
    mytmap_add_edge_l_ffx(edge_lerp + 1, SIDE_RIGHT, top_vtx, right_vtx);
    mytmap_add_edge_l_ffx(edge_lerp + 2, ps->side,   mid_vtx, bottom_vtx);
    
    // and section heights
    section_heights[0] = mid_vtx->cy    - top_vtx->cy;
    section_heights[1] = bottom_vtx->cy - mid_vtx->cy;

    // get display start
    uint8_t* dst_start = mytmap_dst + ((top_vtx->cy) * mytmap_pitch);

    // call common filler
    grad.texture16  = shadetab;
    grad.uvmask     = 0xFF;     // not needed at all? :p

#ifdef USE_INTERP
    MYTMAP_INTERP_SHADETAB->base[0] = grad.dldx;
#endif
    mytmap_draw_sections_affine_l_16(dst_start, 2, top_vtx->cy, section_heights, edge_lerp);
}

// draw arbitrary polygon (expects to be 2D clipped since it's affine mapping)
void mytmap_draw_poly_gouraud_16(tmap_vtx_uv_t *f, uint32_t vtxcount, uint16_t *shadetab) {
    tmap_vtx_uv_t *top_vtx, *bottom_vtx, *mid_vtx, *left_vtx, *right_vtx, *start_vtx, *end_vtx;

    start_vtx   = f;
    end_vtx     = f + vtxcount - 1;

    // find top and bottom vertices
    int top_idx = 0, bottom_idx = 0, mid_idx; int i = 1;
    f[0].cy = ceil(f[0].fp->y);
    do {
        f[i].cy = ceil(f[i].fp->y); // precalc ceil(y)
        if (f[i].fp->y < f[top_idx].fp->y) {
            top_idx = i; 
        } else if (f[i].fp->y > f[bottom_idx].fp->y) {
            bottom_idx = i;
        }
    } while (++i != vtxcount);
    if (top_idx == bottom_idx) return;  // nothing to draw!

    // find middle vertex
    mid_idx = top_idx + 1; if (mid_idx >= vtxcount) mid_idx = 0;
    if (mid_idx == bottom_idx) {
        mid_idx = bottom_idx + 1; if (mid_idx >= vtxcount) mid_idx = 0;
    }
    
    top_vtx     = f + top_idx;
    bottom_vtx  = f + bottom_idx;
    mid_vtx     = f + mid_idx;
    left_vtx    = f + top_idx + 1; if (left_vtx  > end_vtx)   left_vtx   = start_vtx;
    right_vtx   = f + top_idx - 1; if (right_vtx < start_vtx) right_vtx  = end_vtx;

    // calculate gradients
    vec3f *pt = top_vtx->fp, *pm = mid_vtx->fp, *pb = bottom_vtx->fp;
    float inv_height = 1.0f / (pb->y - pt->y);
    float mid_t = (pm->y - pt->y) * inv_height;
    float longest_width = ((pb->x - pt->x) * mid_t) + (pt->x - pm->x);
    if (longest_width == 0.0f) return;     // nothing to draw!
    float inv_width     = 1.0f / longest_width;
    float inv_width_64k = 65536.0f * inv_width;

    float lt = top_vtx->fl, lm = mid_vtx->fl, lb = bottom_vtx->fl;

    // calculate gradients
    grad.dldx  = (int32_t)((((lb - lt) * mid_t) + (lt - lm)) * inv_width_64k);
#ifdef SUBPIXEL_TABLE
    {
        int32_t l = 0;
        int32_t dl = (grad.dldx >> SUBPIXEL_BITS);
        for (int i = 0; i < (1 << SUBPIXEL_BITS); i++) {
            grad.dldx_prestep[i] = l; l += dl;
        }
    }
#endif

    // add first two edges
    mytmap_add_edge_l_ffx(edge_lerp + 0, SIDE_LEFT,  top_vtx, left_vtx);
    mytmap_add_edge_l_ffx(edge_lerp + 1, SIDE_RIGHT, top_vtx, right_vtx);

    // iterate along left and right edges, calculate section heights
    int section_idx = 0;
    tmap_edge_lerp_t *edge_left = edge_lerp + 0, *edge_right = edge_lerp + 1, *edge_cur = edge_lerp + 2;
    int left_height = edge_left->height, right_height = edge_right->height;
    while (((left_vtx != bottom_vtx) || (right_vtx != bottom_vtx)) && (section_idx < vtxcount)) {
        if (left_height < right_height) {
            // next edge is left, adjust left pointer
            section_heights[section_idx++] = left_height;
            right_height -= left_height;
            // add new edge
            tmap_vtx_uv_t *prev_vtx = left_vtx;
            if (left_vtx != bottom_vtx) {
                left_vtx++; if (left_vtx > end_vtx) left_vtx = start_vtx;
                mytmap_add_edge_l_ffx(edge_cur, SIDE_LEFT, prev_vtx, left_vtx);
                edge_left = edge_cur++;
                left_height = edge_left->height;
            }
        } else {
            // next edge is right, adjust right pointer
            section_heights[section_idx++] = right_height;
            left_height -= right_height;
            // add new edge
            tmap_vtx_uv_t *prev_vtx = right_vtx;
            if (right_vtx != bottom_vtx) {
                right_vtx--; if (right_vtx < start_vtx) right_vtx = end_vtx;
                mytmap_add_edge_l_ffx(edge_cur, SIDE_RIGHT, prev_vtx, right_vtx);
                edge_right = edge_cur++;
                right_height = edge_right->height;
            }
        }
    }
    // set last section height
    section_heights[section_idx++] = (left_height < right_height) ? left_height : right_height;

    // get display start
    uint8_t* dst_start = mytmap_dst + ((top_vtx->cy) * mytmap_pitch);

    // call common filler
    grad.texture16  = shadetab;
    grad.uvmask     = 0xFF;     // not needed at all? :p
#ifdef USE_INTERP
    MYTMAP_INTERP_SHADETAB->base[0] = grad.dldx;
#endif
    mytmap_draw_sections_affine_l_16(dst_start, section_idx, top_vtx->cy, section_heights, edge_lerp);
}

// multitexture, 16bpp
void mytmap_draw_tri_multitex_16(tmap_vtx_uv_t *f, uint8_t *tex, uint8_t *tex2, uint16_t *blend) {
    tmap_vtx_uv_t *top_vtx, *bottom_vtx, *mid_vtx, *left_vtx, *right_vtx;

    // calculate index, precompute ceil(y)
    // note type punning here - since Y for all vertices is positive this should be fine
    int index =            ((f[0].p->y < f[1].p->y) ? 1 : 0); f[0].cy = ceil(f[0].fp->y);
    index = (index << 1) | ((f[1].p->y < f[2].p->y) ? 1 : 0); f[1].cy = ceil(f[1].fp->y);
    index = (index << 1) | ((f[2].p->y < f[0].p->y) ? 1 : 0); f[2].cy = ceil(f[2].fp->y);
    if (index == 0) return; // degenerate case

    const tri_setup_t *ps = tri_setup_table + index;

    // get vertex pointers
    top_vtx     = f + ps->top;
    mid_vtx     = f + ps->mid;
    bottom_vtx  = f + ps->bottom;
    left_vtx    = f + ps->left;
    right_vtx   = f + ps->right;

    // calculate gradients
    vec3f *pt = top_vtx->fp, *pm = mid_vtx->fp, *pb = bottom_vtx->fp;
    float inv_height = 1.0f / (pb->y - pt->y);
    float mid_t = (pm->y - pt->y) * inv_height;
    float longest_width = ((pb->x - pt->x) * mid_t) + (pt->x - pm->x);
    if (longest_width == 0.0f) return;     // nothing to draw!
    float inv_width     = 1.0f / longest_width;
    float inv_width_64k = 65536.0f * inv_width;

    vec2f *tt = top_vtx->fuv,  *tm = mid_vtx->fuv,  *tb = bottom_vtx->fuv;
    vec2f *mt = top_vtx->fuv2, *mm = mid_vtx->fuv2, *mb = bottom_vtx->fuv2;

    // prepare texture coordinates
    int32_t scale_u = 256;
    int32_t scale_v = 256;
    for (int vtx = 0; vtx < 3; vtx++) {
        tmap_texcoord[vtx].x = (f[vtx].fuv->x * scale_u);
        tmap_texcoord[vtx].y = (f[vtx].fuv->y * scale_v);
        f[vtx].fuv = tmap_texcoord + vtx;

        tmap_texcoord2[vtx].x = (f[vtx].fuv2->x * scale_u);
        tmap_texcoord2[vtx].y = (f[vtx].fuv2->y * scale_v);
        f[vtx].fuv2 = tmap_texcoord2 + vtx;
    }

    // calculate gradients
    grad.dudx  = (int32_t)((((tb->x - tt->x) * mid_t) + (tt->x - tm->x)) * inv_width_64k * scale_u);
    grad.dvdx  = (int32_t)((((tb->y - tt->y) * mid_t) + (tt->y - tm->y)) * inv_width_64k * scale_v);
    grad.du2dx = (int32_t)((((mb->x - mt->x) * mid_t) + (mt->x - mm->x)) * inv_width_64k * scale_u);
    grad.dv2dx = (int32_t)((((mb->y - mt->y) * mid_t) + (mt->y - mm->y)) * inv_width_64k * scale_v);

    // compute edge sections
    mytmap_add_edge_multiuv_ffx(edge_lerp + 0, SIDE_LEFT,  top_vtx, left_vtx);
    mytmap_add_edge_multiuv_ffx(edge_lerp + 1, SIDE_RIGHT, top_vtx, right_vtx);
    mytmap_add_edge_multiuv_ffx(edge_lerp + 2, ps->side,   mid_vtx, bottom_vtx);
    
    // and section heights
    section_heights[0] = mid_vtx->cy    - top_vtx->cy;
    section_heights[1] = bottom_vtx->cy - mid_vtx->cy;

    // get display start
    uint8_t* dst_start = mytmap_dst + ((top_vtx->cy) * mytmap_pitch);

    // call common filler
    grad.texture    = tex;
    grad.texture2   = tex2;
    grad.blendtab16 = blend;
    grad.uvmask     = 0xFFFF;
    grad.uvmask2    = 0xFFFF;
    mytmap_draw_sections_affine_multiuv_16(dst_start, 2, section_heights, edge_lerp);
}

// draw arbitrary polygon (expects to be 2D clipped since it's affine mapping)
void mytmap_draw_poly_multitex_16(tmap_vtx_uv_t *f, uint32_t vtxcount, uint8_t *tex, uint8_t *tex2, uint16_t *blend) {
    tmap_vtx_uv_t *top_vtx, *bottom_vtx, *mid_vtx, *left_vtx, *right_vtx, *start_vtx, *end_vtx;

    start_vtx   = f;
    end_vtx     = f + vtxcount - 1;

    // find top and bottom vertices
    int top_idx = 0, bottom_idx = 0, mid_idx; int i = 1;
    f[0].cy = ceil(f[0].fp->y);
    do {
        f[i].cy = ceil(f[i].fp->y); // precalc ceil(y)
        if (f[i].fp->y < f[top_idx].fp->y) {
            top_idx = i; 
        } else if (f[i].fp->y > f[bottom_idx].fp->y) {
            bottom_idx = i;
        }
    } while (++i != vtxcount);
    if (top_idx == bottom_idx) return;  // nothing to draw!

    // find middle vertex
    mid_idx = top_idx + 1; if (mid_idx >= vtxcount) mid_idx = 0;
    if (mid_idx == bottom_idx) {
        mid_idx = bottom_idx + 1; if (mid_idx >= vtxcount) mid_idx = 0;
    }
    
    top_vtx     = f + top_idx;
    bottom_vtx  = f + bottom_idx;
    mid_vtx     = f + mid_idx;
    left_vtx    = f + top_idx + 1; if (left_vtx  > end_vtx)   left_vtx   = start_vtx;
    right_vtx   = f + top_idx - 1; if (right_vtx < start_vtx) right_vtx  = end_vtx;

    // calculate gradients
    vec3f *pt = top_vtx->fp, *pm = mid_vtx->fp, *pb = bottom_vtx->fp;
    float inv_height = 1.0f / (pb->y - pt->y);
    float mid_t = (pm->y - pt->y) * inv_height;
    float longest_width = ((pb->x - pt->x) * mid_t) + (pt->x - pm->x);
    if (longest_width == 0.0f) return;     // nothing to draw!
    float inv_width     = 1.0f / longest_width;
    float inv_width_64k = 65536.0f * inv_width;

    vec2f *tt = top_vtx->fuv,  *tm = mid_vtx->fuv,  *tb = bottom_vtx->fuv;
    vec2f *mt = top_vtx->fuv2, *mm = mid_vtx->fuv2, *mb = bottom_vtx->fuv2;

    // prepare texture coordinates
    int32_t scale_u = 256;
    int32_t scale_v = 256;
    for (int vtx = 0; vtx < vtxcount; vtx++) {
        tmap_texcoord[vtx].x = (f[vtx].fuv->x * scale_u);
        tmap_texcoord[vtx].y = (f[vtx].fuv->y * scale_v);
        f[vtx].fuv = tmap_texcoord + vtx;

        tmap_texcoord2[vtx].x = (f[vtx].fuv2->x * scale_u);
        tmap_texcoord2[vtx].y = (f[vtx].fuv2->y * scale_v);
        f[vtx].fuv2 = tmap_texcoord2 + vtx;
    }

    // calculate gradients
    grad.dudx  = (int32_t)((((tb->x - tt->x) * mid_t) + (tt->x - tm->x)) * inv_width_64k * scale_u);
    grad.dvdx  = (int32_t)((((tb->y - tt->y) * mid_t) + (tt->y - tm->y)) * inv_width_64k * scale_v);
    grad.du2dx = (int32_t)((((mb->x - mt->x) * mid_t) + (mt->x - mm->x)) * inv_width_64k * scale_u);
    grad.dv2dx = (int32_t)((((mb->y - mt->y) * mid_t) + (mt->y - mm->y)) * inv_width_64k * scale_v);

    // add first two edges
    mytmap_add_edge_multiuv_ffx(edge_lerp + 0, SIDE_LEFT,  top_vtx, left_vtx);
    mytmap_add_edge_multiuv_ffx(edge_lerp + 1, SIDE_RIGHT, top_vtx, right_vtx);

    // iterate along left and right edges, calculate section heights
    int section_idx = 0;
    tmap_edge_lerp_t *edge_left = edge_lerp + 0, *edge_right = edge_lerp + 1, *edge_cur = edge_lerp + 2;
    int left_height = edge_left->height, right_height = edge_right->height;
    while (((left_vtx != bottom_vtx) || (right_vtx != bottom_vtx)) && (section_idx < vtxcount)) {
        if (left_height < right_height) {
            // next edge is left, adjust left pointer
            section_heights[section_idx++] = left_height;
            right_height -= left_height;
            // add new edge
            tmap_vtx_uv_t *prev_vtx = left_vtx;
            if (left_vtx != bottom_vtx) {
                left_vtx++; if (left_vtx > end_vtx) left_vtx = start_vtx;
                mytmap_add_edge_multiuv_ffx(edge_cur, SIDE_LEFT, prev_vtx, left_vtx);
                edge_left = edge_cur++;
                left_height = edge_left->height;
            }
        } else {
            // next edge is right, adjust right pointer
            section_heights[section_idx++] = right_height;
            left_height -= right_height;
            // add new edge
            tmap_vtx_uv_t *prev_vtx = right_vtx;
            if (right_vtx != bottom_vtx) {
                right_vtx--; if (right_vtx < start_vtx) right_vtx = end_vtx;
                mytmap_add_edge_multiuv_ffx(edge_cur, SIDE_RIGHT, prev_vtx, right_vtx);
                edge_right = edge_cur++;
                right_height = edge_right->height;
            }
        }
    }
    // set last section height
    section_heights[section_idx++] = (left_height < right_height) ? left_height : right_height;

    // get display start
    uint8_t* dst_start = mytmap_dst + ((top_vtx->cy) * mytmap_pitch);

    // call common filler
    grad.texture    = tex;
    grad.texture2   = tex2;
    grad.blendtab16 = blend;
    grad.uvmask     = 0xFFFF;
    grad.uvmask2    = 0xFFFF;
    mytmap_draw_sections_affine_multiuv_16(dst_start, section_idx, section_heights, edge_lerp);
}


}