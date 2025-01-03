#pragma once

#include <stdint.h>
#include "vec.h"

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

// draw triangles
void mytmap_draw_tri_flat(tmap_vtx_uv_t *f, uint32_t color);
void mytmap_draw_tri_flat_16(tmap_vtx_uv_t *f, uint32_t color);
void mytmap_draw_poly_flat_16(tmap_vtx_uv_t *f, uint32_t vtxcount, uint32_t color);
void mytmap_draw_tri_gouraud_16(tmap_vtx_uv_t *f, uint16_t *shadetab);
void mytmap_draw_poly_gouraud_16(tmap_vtx_uv_t *f, uint32_t vtxcount, uint16_t *shadetab);
void mytmap_draw_tri_multitex_16(tmap_vtx_uv_t *f, uint8_t *tex, uint8_t *tex2, uint16_t *blend);
void mytmap_draw_poly_multitex_16(tmap_vtx_uv_t *f, uint32_t vtxcount, uint8_t *tex, uint8_t *tex2, uint16_t *blend);

}
