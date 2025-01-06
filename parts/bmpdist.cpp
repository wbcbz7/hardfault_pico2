#include "bmpdist.h"
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
#include <lxmplay.h>
#include <kucha.h>

#ifdef PICO_BUILD
#include "hardware/interp.h"
#define INTERP_TEXTURE   interp0
#define INTERP_TEXTURE2  interp1
#define INTERP_SHADETAB  interp1
#else
#define interp_hw_t* void
#define INTERP_TEXTURE   NULL
#define INTERP_TEXTURE2  NULL
#define INTERP_SHADETAB  NULL
#endif

#include "../textures/bmpdist_tex.h"

#define GRID_SIZE   8
#define X_GRID      (X_RES/GRID_SIZE)
#define Y_GRID      (Y_RES/GRID_SIZE)
#define X_RES_GRID  (X_GRID*GRID_SIZE)
#define Y_RES_GRID  (Y_GRID*GRID_SIZE)

// grid structure, no lighting yet
struct grid_t {
    int32_t u, v;
};

// texture storage (copied upon part init)
static uint16_t *texsram;

// grid lerp storage
static grid_t *grid;

void bmpdist_init() {
    kucha_reset();
    texsram = (uint16_t*)kucha_alloc(256*256*sizeof(uint16_t));
    grid    = (grid_t*)kucha_alloc(((Y_RES_GRID/GRID_SIZE)+1)*((X_RES_GRID/GRID_SIZE)+1)*sizeof(grid_t));

    memcpy(texsram, bmpdist_texture, sizeof(uint16_t)*256*256);
}

void bmpdist_done()
{
    kucha_reset();
}

// ------------------

static void drawgrid(uint16_t *fb, grid_t *grid, const uint16_t* texture) {
    // copied from my VERY old pc/dos demo lol
    int32_t usy, uey, vsy, vey;
    int32_t usdy, uedy, vsdy, vedy;

    int gridptr = 0;
    uint16_t *p = fb;
    grid_t   *gr = grid;
    
    for (int j = 0; j < Y_GRID; j++) {
        for (int i = 0; i < X_GRID; i++) {
            usdy = (gr[(X_GRID+1)].u - gr[0].u) >> 3;
            usy  = (gr[0].u);
            
            uedy = (gr[0+(X_GRID+1+1)].u - gr[0+1].u) >> 3;
            uey  = (gr[0+1].u);
            
            vsdy = (gr[0+(X_GRID+1)].v - gr[0].v) >> 3;
            vsy  = (gr[0].v);
        
            vedy = (gr[0+(X_GRID+1+1)].v - gr[0+1].v) >> 3;
            vey  = (gr[0+1].v);
            
            int y = GRID_SIZE;
            do {
                INTERP_TEXTURE->accum[0] = usy;
                INTERP_TEXTURE->accum[1] = vsy;
                INTERP_TEXTURE->base [0] = ((uey - usy) >> 3);
                INTERP_TEXTURE->base [1] = ((vey - vsy) >> 3);

                // unroll!
                p[0] = *(const uint16_t*)INTERP_TEXTURE->pop[2];
                p[1] = *(const uint16_t*)INTERP_TEXTURE->pop[2];
                p[2] = *(const uint16_t*)INTERP_TEXTURE->pop[2];
                p[3] = *(const uint16_t*)INTERP_TEXTURE->pop[2];
                p[4] = *(const uint16_t*)INTERP_TEXTURE->pop[2];
                p[5] = *(const uint16_t*)INTERP_TEXTURE->pop[2];
                p[6] = *(const uint16_t*)INTERP_TEXTURE->pop[2];
                p[7] = *(const uint16_t*)INTERP_TEXTURE->pop[2];

                p += X_RES;
                usy += usdy; uey += uedy;
                vsy += vsdy; vey += vedy;
            } while (--y);
            gr++;
            p -= (X_RES * GRID_SIZE) - GRID_SIZE;
        }
        gr++;
        p += (X_RES * (GRID_SIZE-1));
    } 
}

//const float endtime = 10.5f;
const float endtime = 20.7f;

static void calcgrid(grid_t *grid, float t) {
    mat2  rot; 
    rot2(rot, t*1.0f + 0.4*sin(t*0.5f));
    vec2f direction;

    float firstscale;
    if (t < 2.0f) {
        float tt = t/2.0f;
        tt = 1.0 - (1.0-tt)*(1.0-tt);
        firstscale = clamp(1.0f*tt, 0, 1);
    } else
    if (t >= endtime-3.0f) {
        float tt = (t - (endtime-3.0f))/3.0f;
        tt = 1.0 - tt*tt;
        firstscale = clamp(1.0f*tt, 0, 1);
    }
    else {
        firstscale = 1.0f;
    }

    float scale = firstscale * (1.5f + 1.0f*sin(t*0.9f));
    float scale16f = 65536.0;

    float dirvec_scale = (float)Y_RES_GRID / ((float)X_RES_GRID);
    float dirvec_step = dirvec_scale*GRID_SIZE;

    grid_t *p = grid;
    float distscale = firstscale*20.0f*sin(t*0.6f);

    vec2f disp;
    float dispradius = 200.0f;
    disp.x = dispradius*sin(t*0.4f);
    disp.y = dispradius*sin(t*0.4f);

    direction.y = -(Y_RES_GRID/2)*dirvec_scale;
    for (int y = 0; y < (Y_RES_GRID/GRID_SIZE)+1; y++) {
        direction.x = -(X_RES_GRID/2)*dirvec_scale;
        for (int x = 0; x < (X_RES_GRID/GRID_SIZE)+1; x++) {
            float fu, fv;
            
            vec2f uv = {direction.x * scale, direction.y * scale};
            uv = mul(uv, rot);

            uv.x += disp.x;
            uv.y += disp.y;

            uv.x += distscale*(sin(direction.y * 0.025f + t*0.2f) + sin(direction.x * 0.050f + t*0.35f));
            uv.y += distscale*(cos(direction.x * 0.030f + t*0.2f) + cos(direction.y * 0.041f + t*0.25f));

            int32_t u = (int32_t)(uv.x * scale16f);
            int32_t v = (int32_t)(uv.y * scale16f);
            
            p->u = (int32_t)u & 0xFFFFFFFF;
            p->v = (int32_t)v & 0xFFFFFFFF;

            direction.x += dirvec_step;
            p++;
        }
        direction.y += dirvec_step;
    }
}


void bmpdist_run() {
    const uint16_t *texture = texsram;
    //const uint16_t *texture = bmpdist_texture;

    fbIdx = 0;
    uint32_t frame_counter = 0;
    
    // init interpolators
    mytmap_interp_setup_uv(INTERP_TEXTURE, texture, 16, 8, 8, 1);

    while(lxm_current_frame() < (3*16 + 8*3*64)) {
        float t = frame_counter / 60.0f;
        // no need to clear buffer

        calcgrid(grid, t);
        drawgrid(fb[fbIdx], grid, texture);

        // fraw rasterdot and flip buffers
        //rasterdot_xor(argb_to_555(255, 255, 255));
        dvi_set_framebuffer(&fb[fbIdx], 0); fbIdx ^= 1;
        dvi_wait_for_vblank();
        frame_counter++;
    }
}
