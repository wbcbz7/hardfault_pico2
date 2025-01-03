#include "2dclip.h"
#include "vec.h"
#include <stdio.h>
#include <string.h>

#define MAX_VERTICES_CLIPPED 15

// private vertex storage
static vec3f clip_p  [MAX_VERTICES_CLIPPED];
static vec2f clip_uv [MAX_VERTICES_CLIPPED];
static vec2f clip_uv2[MAX_VERTICES_CLIPPED];

static tmap_vtx_uv_t clip_dst[MAX_VERTICES_CLIPPED];

int clippoly(tmap_vtx_uv_t *in, int in_count, int flags, const vec4f *bbox) {
    // first determine if polygon needs clipping at all :)
    int inside = 0, outside = CLIP_BOUNDARY_MASK;

    for (int i = 0; i < in_count; i++) {
        if ((flags & CLIP_LEFT))   if (in[i].fp->x < bbox->x) inside |= CLIP_LEFT;   else outside &= ~CLIP_LEFT;
        if ((flags & CLIP_RIGHT))  if (in[i].fp->x > bbox->z) inside |= CLIP_RIGHT;  else outside &= ~CLIP_RIGHT;
        if ((flags & CLIP_TOP))    if (in[i].fp->y < bbox->y) inside |= CLIP_TOP;    else outside &= ~CLIP_TOP;
        if ((flags & CLIP_BOTTOM)) if (in[i].fp->y > bbox->w) inside |= CLIP_BOTTOM; else outside &= ~CLIP_BOTTOM;
    }

    //printf("%2d %2d\n", inside, outside);

    if (inside == 0)           return in_count;      // fully inside
    if (inside & outside != 0) return 0;             // fully outside

    // else start clipping
    int new_vtx_pos = 0;
    tmap_vtx_uv_t *src = in, *dst = in, *dst0, *tmp;

    // process x1
    if (flags & CLIP_LEFT) {
        src = in, dst = dst0 = clip_dst;
        for (int i = 0; i < in_count; i++) {
            int j = (i + 1); if (j >= in_count) j -= in_count;
            tmap_vtx_uv_t *vi = src + i;
            tmap_vtx_uv_t *vj = src + j;
            
            if (vi->fp->x >= (bbox->x)) {
                *dst++ = *vi;
            }

            if (((vi->fp->x >  (bbox->x)) && (vj->fp->x < (bbox->x))) || 
                ((vj->fp->x >= (bbox->x)) && (vi->fp->x < (bbox->x)))) {
                // alloc new vertex
                float k = (bbox->x - vi->fp->x) / (vj->fp->x - vi->fp->x);
                clip_p[new_vtx_pos].x = bbox->x;
                clip_p[new_vtx_pos].y = vi->fp->y + k * (vj->fp->y - vi->fp->y);
                dst->fp = &clip_p[new_vtx_pos];

                if (flags & CLIP_FLAGS_L) {
                    dst->fl = vi->fl + k * (vj->fl - vi->fl);
                }
                if (flags & CLIP_FLAGS_UV) {
                    clip_uv[new_vtx_pos].x = vi->fuv->x + k * (vj->fuv->x - vi->fuv->x);
                    clip_uv[new_vtx_pos].y = vi->fuv->y + k * (vj->fuv->y - vi->fuv->y);
                    dst->fuv = &clip_uv[new_vtx_pos];
                }
                if (flags & CLIP_FLAGS_UV2) {
                    clip_uv2[new_vtx_pos].x = vi->fuv2->x + k * (vj->fuv2->x - vi->fuv2->x);
                    clip_uv2[new_vtx_pos].y = vi->fuv2->y + k * (vj->fuv2->y - vi->fuv2->y);
                    dst->fuv2 = &clip_uv2[new_vtx_pos];
                }
                new_vtx_pos++;
                dst++;
            }
        }
        in_count = dst - dst0;
    } else memcpy(clip_dst, in, sizeof(tmap_vtx_uv_t)*in_count);

    // process x2
    if (flags & CLIP_RIGHT) {
        src = clip_dst, dst = dst0 = in;
        for (int i = 0; i < in_count; i++) {
            int j = (i + 1); if (j >= in_count) j -= in_count;
            tmap_vtx_uv_t *vi = src + i;
            tmap_vtx_uv_t *vj = src + j;
            
            if (vi->fp->x < (bbox->z)) {
                *dst++ = *vi;
            }

            if (((vi->fp->x <  (bbox->z)) && (vj->fp->x >= (bbox->z))) || 
                ((vj->fp->x <  (bbox->z)) && (vi->fp->x >= (bbox->z)))) {
                // alloc new vertex
                float k = (bbox->z - vi->fp->x) / (vj->fp->x - vi->fp->x);
                clip_p[new_vtx_pos].x = bbox->z;
                clip_p[new_vtx_pos].y = vi->fp->y + k * (vj->fp->y - vi->fp->y);
                dst->fp = &clip_p[new_vtx_pos];

                if (flags & CLIP_FLAGS_L) {
                    dst->fl = vi->fl + k * (vj->fl - vi->fl);
                }
                if (flags & CLIP_FLAGS_UV) {
                    clip_uv[new_vtx_pos].x = vi->fuv->x + k * (vj->fuv->x - vi->fuv->x);
                    clip_uv[new_vtx_pos].y = vi->fuv->y + k * (vj->fuv->y - vi->fuv->y);
                    dst->fuv = &clip_uv[new_vtx_pos];
                }
                if (flags & CLIP_FLAGS_UV2) {
                    clip_uv2[new_vtx_pos].x = vi->fuv2->x + k * (vj->fuv2->x - vi->fuv2->x);
                    clip_uv2[new_vtx_pos].y = vi->fuv2->y + k * (vj->fuv2->y - vi->fuv2->y);
                    dst->fuv2 = &clip_uv2[new_vtx_pos];
                }
                new_vtx_pos++;
                dst++;
            }
        }
        in_count = dst - dst0;
    } else memcpy(clip_dst, in, sizeof(tmap_vtx_uv_t)*in_count);

    // process y1
    if (flags & CLIP_TOP) {
        src = in, dst = dst0 = clip_dst;
        for (int i = 0; i < in_count; i++) {
            int j = (i + 1); if (j >= in_count) j -= in_count;
            tmap_vtx_uv_t *vi = src + i;
            tmap_vtx_uv_t *vj = src + j;
            
            if (vi->fp->y >= (bbox->y)) {
                *dst++ = *vi;
            }

            if (((vi->fp->y >  (bbox->y)) && (vj->fp->y < (bbox->y))) || 
                ((vj->fp->y >= (bbox->y)) && (vi->fp->y < (bbox->y)))) {
                // alloc new vertex
                float k = (bbox->y - vi->fp->y) / (vj->fp->y - vi->fp->y);
                clip_p[new_vtx_pos].y = bbox->y;
                clip_p[new_vtx_pos].x = vi->fp->x + k * (vj->fp->x - vi->fp->x);
                dst->fp = &clip_p[new_vtx_pos];

                if (flags & CLIP_FLAGS_L) {
                    dst->fl = vi->fl + k * (vj->fl - vi->fl);
                }
                if (flags & CLIP_FLAGS_UV) {
                    clip_uv[new_vtx_pos].x = vi->fuv->x + k * (vj->fuv->x - vi->fuv->x);
                    clip_uv[new_vtx_pos].y = vi->fuv->y + k * (vj->fuv->y - vi->fuv->y);
                    dst->fuv = &clip_uv[new_vtx_pos];
                }
                if (flags & CLIP_FLAGS_UV2) {
                    clip_uv2[new_vtx_pos].x = vi->fuv2->x + k * (vj->fuv2->x - vi->fuv2->x);
                    clip_uv2[new_vtx_pos].y = vi->fuv2->y + k * (vj->fuv2->y - vi->fuv2->y);
                    dst->fuv2 = &clip_uv2[new_vtx_pos];
                }
                new_vtx_pos++;
                dst++;
            }
        }
        in_count = dst - dst0;
    } else memcpy(clip_dst, in, sizeof(tmap_vtx_uv_t)*in_count);

    // process y2
    if (flags & CLIP_BOTTOM) {
        src = clip_dst, dst = dst0 = in;
        for (int i = 0; i < in_count; i++) {
            int j = (i + 1); if (j >= in_count) j -= in_count;
            tmap_vtx_uv_t *vi = src + i;
            tmap_vtx_uv_t *vj = src + j;
            
            if (vi->fp->y < (bbox->w)) {
                *dst++ = *vi;
            }

            if (((vi->fp->y <  (bbox->w)) && (vj->fp->y >= (bbox->w))) || 
                ((vj->fp->y <  (bbox->w)) && (vi->fp->y >= (bbox->w)))) {
                // alloc new vertex
                float k = (bbox->w - vi->fp->y) / (vj->fp->y - vi->fp->y);
                clip_p[new_vtx_pos].y = bbox->w;
                clip_p[new_vtx_pos].x = vi->fp->x + k * (vj->fp->x - vi->fp->x);
                dst->fp = &clip_p[new_vtx_pos];

                if (flags & CLIP_FLAGS_L) {
                    dst->fl = vi->fl + k * (vj->fl - vi->fl);
                }
                if (flags & CLIP_FLAGS_UV) {
                    clip_uv[new_vtx_pos].x = vi->fuv->x + k * (vj->fuv->x - vi->fuv->x);
                    clip_uv[new_vtx_pos].y = vi->fuv->y + k * (vj->fuv->y - vi->fuv->y);
                    dst->fuv = &clip_uv[new_vtx_pos];
                }
                if (flags & CLIP_FLAGS_UV2) {
                    clip_uv2[new_vtx_pos].x = vi->fuv2->x + k * (vj->fuv2->x - vi->fuv2->x);
                    clip_uv2[new_vtx_pos].y = vi->fuv2->y + k * (vj->fuv2->y - vi->fuv2->y);
                    dst->fuv2 = &clip_uv2[new_vtx_pos];
                }
                new_vtx_pos++;
                dst++;
            }
        }
        in_count = dst - dst0;
    } else memcpy(in, clip_dst, sizeof(tmap_vtx_uv_t)*in_count);

    return in_count;
}

