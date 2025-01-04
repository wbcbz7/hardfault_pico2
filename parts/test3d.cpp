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

#include "../objects/duck3ds.h"

enum {
    MAX_VERTICES            = 500,
    MAX_NORMALS             = 500,
    MAX_VERTICES_PER_TRI    = 10,
    MAX_FACES               = 650,
};

// face sorting struct
static face_sort_t *facesort;//[MAX_FACES];
static face_sort_t *facesort_tmp;//[MAX_FACES];

// transformed vertices storage
static vec3f *vt;//[MAX_VERTICES];

// gouraud shading table
static uint16_t shadetab[2][256];

void test3d_init()
{
    vt = new vec3f[MAX_VERTICES];
    facesort = new face_sort_t[MAX_FACES];
    facesort_tmp = new face_sort_t[MAX_FACES];

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
    delete[] vt;
    delete[] facesort;
    delete[] facesort_tmp;
}

void test3d_run()
{
    const incobj_t *obj = duck3ds_object;

    static const float FOV = 160.0f;
    static vec4f bbox = {.x = 0, .y = 0, .z = X_RES-1, .w = Y_RES-1};

    // calculate shading table

    fbIdx = 0;
    uint32_t frame_counter = 0;

    while(1) {
        float t = frame_counter / 60.0f;
        fb_fill(&fb[fbIdx], 0, X_RES*Y_RES);
        rasterdot(argb_to_555(0, 0, 255));
        mytmap_polydraw_init(&fb[fbIdx], X_RES*BYTES_PER_PIXEL);
#if 1
        vec3f cam = {0, 0.3, 2.6};
        mat4 view, view_inv;
        mat4 m_rot; rot4(m_rot, 0.3*sin(t*1.2), t*0.9, 0.0);
        mat4 m_ofs; ofs4(m_ofs, cam.x, cam.y, cam.z);
#else
        vec3f cam = {1*sin(t*0.7), 1*sin(t*0.6), 2.6};
        //vec3f cam = {0, 0, 2.6};
        mat4 view, view_inv;

        mat4 m_rot; rot4(m_rot, t*0.6, t*0.7, t*0.9);
        //mat4 m_rot; rot4(m_rot, 0,0, t*0.9);
        mat4 m_ofs; ofs4(m_ofs, cam.x, cam.y, cam.z);
#endif
        matmul4(view, m_ofs, m_rot);
        inv4x3(view_inv, view);

        // object-space lighting vector
        vec3f l = {0, 0, -1};    // lighting vector
        vec3f os_l; mulrs(os_l, l, view_inv);        // transform face normal

        // object-space camera origin vector
        vec3f os_cam = {view_inv[3], view_inv[7], view_inv[11]};

        int faces_to_draw = 0, vtx_pos = 0;

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
        //face_sort_radix(facesort, facesort_tmp, faces_to_draw);
#endif
        rasterdot(argb_to_555(255, 0, 255));

#if 1
        // setup HW interpolators
        mytmap_interp_setup_l_2x2(MYTMAP_INTERP_SHADETAB, shadetab, 16, 8, 0, 1);
#endif

#if 1
        // draw flat faces!
        // draw faces, without any particular order
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
#if 1
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
#endif
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
