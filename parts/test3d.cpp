#include "test3d.h"
#include <stdint.h>
#include <vec.h>
#include <dvi.h>
#include <fbstuff.h>
#include <defs.h>
#include <argb.h>
#include <mytmap.h>
#include <2dclip.h>
#include <facesort.h>
#include <matrix.h>
#include <incobj.h>
#include <string.h>
#include <palerp.h>
#include <timer.h>
#include <kucha.h>
#include <lxmplay.h>

#include "../objects/duck3ds.h"
#include "../objects/torus2.h"

#include "../textures/owl.h"

#include "../textures/env_tex.h"
#include "../textures/env_envmap.h"
#include "../textures/env_blendtab.h"

#include "../textures/bgmap_tex.h"
#include "../textures/bgmap_tiles.h"

enum {
    MAX_VERTICES            = 500,
    MAX_NORMALS             = 500,
    MAX_VERTICES_PER_TRI    = 10,
    MAX_FACES               = 650,
};

// face sorting struct
static face_sort_t *facesort;//[MAX_FACES];

// transformed vertices/normals storage
static vec3f *vt;//[MAX_VERTICES];
static vec3f *nt;//[MAX_NORMALS];

// gouraud shading table
static uint16_t shadetab[2][256];

// texutre and envmap
static uint8_t  *texture;
static uint8_t  *envmap;
static uint16_t *phongtab;

// bgmap stuff
// tilemap texture
uint8_t  *bgtexture;//[TEXTURE_SIZE*TEXTURE_SIZE];
// mapping table
uint16_t *bgmappingtab;//[X_GRID*Y_GRID];
// tile graphics
uint16_t *bgtilegfx;//[MAX_TILES][GRID_SIZE*GRID_SIZE];

#define MAX_TILES   10
#define MAX_TILES   10
#define GRID_SIZE   8
#define X_GRID      (X_RES/GRID_SIZE)
#define Y_GRID      (Y_RES/GRID_SIZE)
#define X_RES_GRID  (X_GRID*GRID_SIZE)
#define Y_RES_GRID  (Y_GRID*GRID_SIZE)

#define TEXTURE_SIZE_LOG2 5
#define TEXTURE_SIZE (1 << TEXTURE_SIZE_LOG2)

static void bg_calctab();
static void bg_calctiles();
static void bg_calctex();

void test3d_init()
{
    kucha_reset();
    vt = (vec3f*)kucha_alloc(sizeof(vec3f)*MAX_VERTICES);
    nt = (vec3f*)kucha_alloc(sizeof(vec3f)*MAX_NORMALS);
    facesort = (face_sort_t*)kucha_alloc(sizeof(face_sort_t)*MAX_FACES);

    texture   = (uint8_t*)kucha_alloc(sizeof(uint8_t)*(256*256));
    envmap    = (uint8_t*)kucha_alloc(sizeof(uint8_t)*(256*256));
    phongtab  = (uint16_t*)kucha_alloc(sizeof(uint16_t)*(64*256));

    bgtexture    = (uint8_t*)kucha_alloc(sizeof(uint8_t)*(TEXTURE_SIZE*TEXTURE_SIZE));
    bgmappingtab = (uint16_t*)kucha_alloc(sizeof(uint16_t)*(X_GRID*Y_GRID));
    bgtilegfx    = (uint16_t*)kucha_alloc(sizeof(uint16_t)*(MAX_TILES*GRID_SIZE*GRID_SIZE));

    memcpy(texture, env_texture, sizeof(uint8_t)*256*256);
    memcpy(envmap, env_envmap, sizeof(uint8_t)*256*256);
    memcpy(phongtab, env_blendtab, sizeof(uint16_t)*64*256);

    bg_calctex();
    bg_calctiles();
    bg_calctab();

    // calculate shading table
    argb32 c0, c1;
#if 1
    c0.r=60; c0.g=40+4; c0.b=40+2; c1.r=255; c1.g=240, c1.b=96;
    pal_lerp_single_rgb555_256(shadetab[1], c0, c1);
    c0.r=20; c0.g=20+4; c0.b=20+2; c1.r=40; c1.g=40, c1.b=40;
    pal_lerp_single_rgb555_256(shadetab[0], c0, c1);
#else
    c0.r=60; c0.g=40+4; c0.b=40+2; c1.r=255; c1.g=32, c1.b=32;
    pal_lerp_single_rgb555_256(shadetab[1], c0, c1);
    c0.r=40; c0.g=40+4; c0.b=40+2; c1.r=255; c1.g=255, c1.b=255;
    pal_lerp_single_rgb555_256(shadetab[0], c0, c1);
#endif
}

void test3d_done()
{
    kucha_reset();
}

static void bg_calctab() {
    uint16_t *t = bgmappingtab;
    for (int y = -Y_GRID/2; y < Y_GRID/2; y++) {
        for (int x = -X_GRID/2; x < X_GRID/2; x++) {
#if 1
            // calculate radius and angle first, and normalize to [1; 1] range
            float r = (sqrt(x*x + y*y) + 1e-6f);
            float a = 1.5f * (1.0f * (atan2(y, x) + pi)) / pi;
            
            // then calculate U and V factors
            float u = 100.0f/r;
            float v = TEXTURE_SIZE * (a);
            
            int iu = u;
            int iv = v;
#else
            // calculate radius and angle first, and normalize to [1; 1] range
            float r = (sqrt(x*x + y*y) + 1e-6f);
            float a = 1.5f * (1.0f * (atan2(y, x) + pi)) / pi;
            
            // then calculate U and V factors
            float u = pow(r, 1.1f);
            float v = TEXTURE_SIZE * (a + (pi/8.0f)*sin(2.0f*a*pi));
            
            int iu = -u;
            int iv = v;
#endif
            *t++ = ((iv & 0x1F) << 5) | ((iu) & 0x1F);
        }
    }
}

static void bg_calctex() {
    uint8_t *t = bgtexture;
    for (int y = -TEXTURE_SIZE/2; y < TEXTURE_SIZE/2; y++) {
        for (int x = -TEXTURE_SIZE/2; x < TEXTURE_SIZE/2; x++) {
            //int r = (x ^ y) & 7;
            //*t++ = r;
            float r = sin(sqrt(x*x + y*y)*0.2);
            *t++ = ((r * 4.5) + 5);
        }
    }
}

static void bg_calctiles() {
    const uint8_t *p = bgmap_tiles;
    for (int gl = 0; gl < MAX_TILES; gl++) {
        uint16_t *tile = bgtilegfx + (gl*GRID_SIZE*GRID_SIZE);
        for (int y = 0; y < GRID_SIZE; y++) {
            for (int x = 0; x < GRID_SIZE; x++) {
                uint8_t a = *p++;
                *tile++ = a > 0 ? argb_to_555(32, 80, 64) : argb_to_555(24, 32, 24);
            }
        }
    }
}

extern "C" void bgmap_copy_8x8(uint16_t *dst, uint16_t *src, uint32_t dstfixup);

static void bg_map(uint16_t *fb, uint32_t tabofs) {
    uint32_t texmask = (1 << (TEXTURE_SIZE_LOG2*2)) - 1;
    uint16_t *p = fb;
    uint16_t *tab = bgmappingtab;
    for (int j = 0; j < Y_GRID; j++) {
        for (int i = 0; i < X_GRID; i++) {
            uint16_t *tile = &bgtilegfx[bgtexture[(*tab++ + tabofs) & texmask]*GRID_SIZE*GRID_SIZE];
            bgmap_copy_8x8(p, tile, (X_RES - GRID_SIZE)*BYTES_PER_PIXEL);
            p += GRID_SIZE;
        }
        p += (X_RES * (GRID_SIZE-1));
    }
}

enum {
    STATE_DUCK3DS,
    STATE_TORUS
};

void test3d_run()
{
    int state = STATE_DUCK3DS;
    vec3f oo = {0.0f, -4.0f, 0.0f};

    static const float FOV = 160.0f;
    static vec4f bbox = {.x = 0, .y = 0, .z = X_RES-1, .w = Y_RES-1};

    // calculate shading table
    fbIdx = 0;
    uint32_t frame_counter = 0;

    int deltalxm = (3*16 + 12*3*64);
    //int deltalxm = (3*16);

    float t; volatile float ot = ftimer_get(); float dt;
    while(lxm_current_frame() < (deltalxm + 4*3*64)) {
        ot = t; t = ftimer_get(); dt = t - ot;
        {
            int u = TEXTURE_SIZE*t*1.0f;
            int v = TEXTURE_SIZE*(1.5f*sin(t*1.0f) + cos(t*0.8f));
            uint32_t tabofs = (u & (TEXTURE_SIZE-1)) | ((v & (TEXTURE_SIZE-1)) << TEXTURE_SIZE_LOG2);
            bg_map(fb[fbIdx], tabofs);
        }
        rasterdot(argb_to_555(0, 0, 255));
        mytmap_polydraw_init(&fb[fbIdx], X_RES*BYTES_PER_PIXEL);
    
        int lxmf = lxm_current_frame();
        // sync :D
        if (lxmf < (deltalxm + 3*32)) {
            oo.y += dt*2.75f;
            if (oo.y >= 0.0f) oo.y = 0.0f;
        }
        if ((lxmf > (deltalxm + 1*3*64 + 3*48)) && (lxmf < (deltalxm + 2*3*64))) {
            oo.y += dt*3.0f;
        }
        if ((lxmf > (deltalxm + 2*3*64 + 3*0)) && (lxmf < (deltalxm + 2*3*64 + 3*32))) {
            oo.y -= dt*3.0f;
            if (oo.y <= 0.0f) oo.y = 0.0f;
        }
        if ((lxmf > (deltalxm + 3*3*64 + 3*48)) && (lxmf < (deltalxm + 3*3*64 + 3*64))) {
            oo.z -= dt*0.75f;
        }

        state = lxmf < (deltalxm + 2*3*64) ? STATE_DUCK3DS : STATE_TORUS;

        vec3f cam; mat4 view, view_inv;
        mat4 m_rot; mat4 m_ofs; 
        if (state == STATE_DUCK3DS) {
            cam.x = 0;
            cam.y = 0.3;
            cam.z = 2.6;
            cam += oo;
            
            rot4r(m_rot, 0.5*sin(t*1.7), t*1.9, 0.2*sin(t*0.7));
            ofs4(m_ofs, cam.x, cam.y, cam.z);
        } else {
            cam.x = 0.7f*sin((t*1.5f) - ((2*3*64 + 3*16)/36.0f));
            cam.y = 0.7f*sin((t*1.3f) - ((2*3*64 + 3*16)/36.0f));
            cam.z = 2.6f;
            cam += oo;

            rot4(m_rot, t*1.0, t*1.2, t*1.4);
            ofs4(m_ofs, cam.x, cam.y, cam.z);
        }

        matmul4(view, m_ofs, m_rot);
        inv4x3(view_inv, view);

        // object-space lighting vector
        vec3f l = {0, 0, -1};    // lighting vector
        vec3f os_l; mulrs(os_l, l, view_inv);        // transform face normal

        // object-space camera origin vector
        vec3f os_cam = {view_inv[3], view_inv[7], view_inv[11]};

        int faces_to_draw = 0, vtx_pos = 0;

        const incobj_t *obj;
        if (state == STATE_DUCK3DS) {
            obj = duck3ds_object;
        } else {
            obj = torus2_object;
        }

        // transform
        for (int i = 0; i < obj->total_pos; i++) {
            mulfr(vt[i], obj->p[i], view);
        }
        for (int i = 0; i < obj->total_pos; i++) {
            float rz = FOV / vt[i].z;
            vt[i].x = vt[i].x*rz + X_RES/2;
            vt[i].y = vt[i].y*rz + Y_RES/2;
            vt[i].z = rz * (1.0f / FOV);
        }
        vtx_pos += obj->total_pos;
        for (int i = 0; i < obj->total_normals; i++) {
            // go straight to texture coordinates lol
            vec3f n; mulrs(n, obj->n[i], view);
            nt[i].x = ((n.x + 1))* 0.5f;
            nt[i].y = ((n.y + 1))*-0.5f;
        }
        rasterdot(argb_to_555(255, 0, 0));

#if 1
        // do both back face culling and putting faces to sorting list
        {
            face_sort_t *fs = facesort;
            const incobj_face_t *objf = obj->f;
            for (int i = 0; i < obj->total_faces; i++) {
                // back face culling
                vec3f p0; sub3f(p0, os_cam, obj->p[obj->i[objf->start].p]);
                if (dot(p0, objf->fn) >= 0.0f) {
                    // calculate average Z
                    const incobj_idx_t *idx = obj->i + objf->start;
                    float avg_z = (vt[idx[0].p].z+vt[idx[1].p].z+vt[idx[2].p].z)*(65536.0f/3.0f);
                    fs->depth = avg_z;
                    fs->face  = objf;
                    fs++;
                }
                objf++;
            }
            faces_to_draw = fs - facesort;
        }
        face_sort(facesort, faces_to_draw);
#endif
        rasterdot(argb_to_555(255, 0, 255));

        // setup HW interpolators
        if (state == STATE_DUCK3DS) {
            mytmap_interp_setup_l_2x2(MYTMAP_INTERP_SHADETAB, shadetab, 16, 8, 0, 1);
        } else {
            mytmap_interp_setup_uv(MYTMAP_INTERP_TEXTURE,  texture, 16, 8, 8, 0);
            mytmap_interp_setup_uv(MYTMAP_INTERP_TEXTURE2, envmap,  16, 8, 8, 0);
        }

#if 1
        // draw faces
        const face_sort_t *fs = facesort;
        for (int i = 0; i < faces_to_draw; i++) {
            const incobj_face_t *objf = fs->face;
            static tmap_vtx_uv_t ff[MAX_VERTICES_PER_TRI];
            // static storage for clipper
            const incobj_idx_t *idx = obj->i + objf->start;
            for (int vtx = 0; vtx < objf->length; vtx++) {
                ff[vtx].fp  = vt + idx[vtx].p;
                ff[vtx].fuv = (vec2f*)obj->t + idx[vtx].t;
            }

#if 0
            // flat shading
            float dotNL = max(dot(objf->fn, os_l), 0.0f);
            int color = (int)(dotNL * 31) * 0x421;
            //int color = 0x7FFF;
            //mytmap_draw_tri_flat_16(&ff[0], (int)(dotNL * 31) * 0x421);
            //mytmap_draw_poly_flat_16(ff, 3, (int)(dotNL * 31) * 0x421);
            int poly_count = clippoly(ff, 3, CLIP_BOUNDARY_MASK, &bbox);
            switch(poly_count) {
                case 0: case 1: case 2: break;
                case 3:     mytmap_draw_tri_flat_16(ff, color); break;
                default:    mytmap_draw_poly_flat_16(ff, poly_count, color); break;
            } 
#endif
#if 0
            // texture mapping
            mytmap_interp_set_texture(MYTMAP_INTERP_TEXTURE, texture_owl);
            {
                int poly_count = clippoly(ff, 3, CLIP_BOUNDARY_MASK | CLIP_FLAGS_UV, &bbox);
                switch(poly_count) {
                    case 0: case 1: case 2: break;
                    case 3:     mytmap_draw_tri_tex_16 (ff, (uint16_t *)texture_owl); break;
                    default:    mytmap_draw_poly_tex_16(ff, poly_count, (uint16_t *)texture_owl); break;
                }
            }
#endif
            if (state == STATE_DUCK3DS) {
                // gouraud shading
                for (int vtx = 0; vtx < objf->length; vtx++) {
                    float dotNL = max(dot(obj->n[idx[vtx].n], os_l), 0.02f);
                    //float l = 0.4*dotNL + 0.6*pow(dotNL,16.0);
                    //float dotNL = ff[vtx].fp->z * 0.7;
                    ff[vtx].fl = dotNL * 250; // fix dithering
                }
                mytmap_interp_set_texture(MYTMAP_INTERP_SHADETAB, shadetab[objf->mat]);
                int poly_count = clippoly(ff, 3, CLIP_BOUNDARY_MASK | CLIP_FLAGS_L, &bbox);
                switch(poly_count) {
                    case 0: case 1: case 2: break;
                    case 3:     mytmap_draw_tri_gouraud_16 (ff, shadetab[objf->mat]); break;
                    default:    mytmap_draw_poly_gouraud_16(ff, poly_count, shadetab[objf->mat]); break;
                } 
            }
            else {
                // tmap with PHONG environment mapping!!
                vec2f uv[3];
                for (int vtx = 0; vtx < objf->length; vtx++) {
                    ff[vtx].fuv2 = (vec2f*)&nt[idx[vtx].n];
                }
                int poly_count = clippoly(ff, 3, CLIP_BOUNDARY_MASK | CLIP_FLAGS_UV | CLIP_FLAGS_UV2, &bbox);
                switch(poly_count) {
                    case 0: case 1: case 2: break;
                    case 3:     mytmap_draw_tri_multitex_16 (
                        ff,
                        (uint8_t*) texture,
                        (uint8_t*) envmap,
                        (uint16_t*)phongtab
                    ); break;
                    default:    mytmap_draw_poly_multitex_16(
                        ff,
                        poly_count,
                        (uint8_t*) texture,
                        (uint8_t*) envmap,
                        (uint16_t*)phongtab
                    ); break;
                }
            }
            fs++;
        }
#endif

#if 0
        vec3f *vp = vt;
        for (int v = 0; v < vtx_pos; v++) {
            if (vp->x >= 0 && vp->x < X_RES && vp->y >= 0 && vp->y < Y_RES) {
                fb[fbIdx][(int)vp->y*X_RES + (int)vp->x] = 0x7FFF;
            }
            vp++;
        }
#endif
        // draw "rasterbar"
        rasterdot(argb_to_555(255, 255, 255));
        dvi_set_framebuffer(&fb[fbIdx], 0); fbIdx ^= 1;
        dvi_wait_for_vblank();
        frame_counter++;
    }
}
