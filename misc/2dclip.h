#pragma once 
#include <stdint.h>
#include "mytmap.h"

// additional properties to be clipped
enum {
    CLIP_LEFT           = (1 << 0),
    CLIP_RIGHT          = (1 << 1),
    CLIP_TOP            = (1 << 2),
    CLIP_BOTTOM         = (1 << 3),
    CLIP_BOUNDARY_MASK  = (1 << 4)-1,

    CLIP_FLAGS_L        = (1 << 5),
    CLIP_FLAGS_UV       = (1 << 6),
    CLIP_FLAGS_UV2      = (1 << 7),
};

// clip polygon against a box, returns number of vertices in a polygon
// NB: uses private memory storage for new vertices, so draw clipped polygon befor feeding the next!
int clippoly(tmap_vtx_uv_t *src, int in_count, int flags, const vec4f *bbox);

