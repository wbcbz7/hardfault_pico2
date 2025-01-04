#pragma once

#include <stdint.h>
#include <defs.h>
#include "vec.h"

#ifdef PICO_BUILD
#include "hardware/interp.h"
#define MYTMAP_INTERP_TEXTURE   interp0
#define MYTMAP_INTERP_TEXTURE2  interp1
#define MYTMAP_INTERP_SHADETAB  interp1
#else
#define interp_hw_t* void
#define MYTMAP_INTERP_TEXTURE   NULL
#define MYTMAP_INTERP_TEXTURE2  NULL
#define MYTMAP_INTERP_SHADETAB  NULL
#endif

extern "C" {

struct tmap_vtx_uv_t {
    union {vec3x* p;  vec3f *fp;};
    union {vec2x* uv; vec2f *fuv;};
    union {
        union {vec2x*  uv2; vec2f *fuv2;};
        union {int32_t l;   float fl;};
    };
    int32_t cy; // precomputed ceil(y)
};

// init tmapper
void mytmap_polydraw_init(void *buf, int pitch);

// init tmap interpolators
void mytmap_interp_setup_l    (interp_hw_t *interp, const void *texture, uint32_t fract_bits, uint32_t width_bits, uint32_t height_bits, uint32_t bit_bias);
void mytmap_interp_setup_l_2x2(interp_hw_t *interp, const void *texture, uint32_t fract_bits, uint32_t width_bits, uint32_t height_bits, uint32_t bit_bias);
void mytmap_interp_setup_uv   (interp_hw_t *interp, const void *texture, uint32_t fract_bits, uint32_t width_bits, uint32_t height_bits, uint32_t bit_bias);
#ifdef PICO_BUILD
inline void mytmap_interp_set_texture(interp_hw_t *interp, const void *texture) {
    interp->base[2] = (uintptr_t)texture;
}
#else
#define mytmap_interp_set_texture(interp, texture) {}
#endif

// draw triangles
void mytmap_draw_tri_flat(tmap_vtx_uv_t *f, uint32_t color);
void mytmap_draw_tri_flat_16(tmap_vtx_uv_t *f, uint32_t color);
void mytmap_draw_poly_flat_16(tmap_vtx_uv_t *f, uint32_t vtxcount, uint32_t color);
void mytmap_draw_tri_gouraud_16(tmap_vtx_uv_t *f, uint16_t *shadetab);
void mytmap_draw_poly_gouraud_16(tmap_vtx_uv_t *f, uint32_t vtxcount, uint16_t *shadetab);

void mytmap_draw_tri_tex_16(tmap_vtx_uv_t *f, uint16_t *tex);
void mytmap_draw_poly_tex_16(tmap_vtx_uv_t *f, uint32_t vtxcount, uint16_t *tex);

void mytmap_draw_tri_multitex_16(tmap_vtx_uv_t *f, uint8_t *tex, uint8_t *tex2, uint16_t *blend);
void mytmap_draw_poly_multitex_16(tmap_vtx_uv_t *f, uint32_t vtxcount, uint8_t *tex, uint8_t *tex2, uint16_t *blend);

}
