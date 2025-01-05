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
#include <kucha.h>
#include <lxmplay.h>

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

#include "../textures/tunnel_tex.h"
#include "../textures/tunnel_blendtab.h"

#define GRID_SIZE   8
#define X_GRID      (X_RES/GRID_SIZE)
#define Y_GRID      (Y_RES/GRID_SIZE)
#define X_RES_GRID  (X_GRID*GRID_SIZE)
#define Y_RES_GRID  (Y_GRID*GRID_SIZE)

// grid structure, no lighting yet
struct grid_t {
    int32_t u, v, l;
};

// texture storage (copied upon part init)
static uint8_t  *texsram;
static uint16_t *shadetabsram;

// grid lerp storage
static grid_t *grid;


void tunnel_init() {
    kucha_reset();
    texsram      = (uint8_t*)kucha_alloc(sizeof(uint8_t)*(256*256));
    shadetabsram = (uint16_t*)kucha_alloc(sizeof(uint16_t)*(64*256));
    grid = (grid_t*)kucha_alloc(sizeof(grid_t)*((Y_RES_GRID/GRID_SIZE)+1)*((X_RES_GRID/GRID_SIZE)+1));

    memcpy(texsram, tunnel_texture, sizeof(uint8_t)*256*256);
    memcpy(shadetabsram, tunnel_blendtab, sizeof(uint16_t)*64*256);
}

void tunnel_done()
{
    kucha_reset();
}

// ------------------

static void drawgrid(uint16_t *fb, grid_t *grid, const uint16_t* texture) {
    // copied from my VERY old pc/dos demo lol
    int32_t usy, uey, vsy, vey, lsy, ley;
    int32_t usdy, uedy, vsdy, vedy, lsdy, ledy;

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

            lsdy = (gr[0+(X_GRID+1)].l - gr[0].l) >> 3;
            lsy  = (gr[0].l);
        
            ledy = (gr[0+(X_GRID+1+1)].l - gr[0+1].l) >> 3;
            ley  = (gr[0+1].l);
            
            int y = GRID_SIZE;
            do {
                INTERP_TEXTURE ->accum[0] = usy;
                INTERP_TEXTURE ->accum[1] = vsy;
                INTERP_SHADETAB->accum[0] = lsy;
                INTERP_TEXTURE ->base [0] = ((uey - usy) >> 3);
                INTERP_TEXTURE ->base [1] = ((vey - vsy) >> 3);
                INTERP_SHADETAB->base [0] = ((ley - lsy) >> 3);

#if 0
                for (int x = 0; x < GRID_SIZE; x++) {
                    INTERP_SHADETAB->accum[1] = *(const uint8_t*)INTERP_TEXTURE->pop[2];
                    p[x] = *(const uint16_t*)INTERP_SHADETAB->pop[2];
                };
                
#else
                // unroll!
                INTERP_SHADETAB->accum[1] = *(const uint8_t*)INTERP_TEXTURE->pop[2];
                p[0] = *(const uint16_t*)INTERP_SHADETAB->pop[2];
                INTERP_SHADETAB->accum[1] = *(const uint8_t*)INTERP_TEXTURE->pop[2];
                p[1] = *(const uint16_t*)INTERP_SHADETAB->pop[2];
                INTERP_SHADETAB->accum[1] = *(const uint8_t*)INTERP_TEXTURE->pop[2];
                p[2] = *(const uint16_t*)INTERP_SHADETAB->pop[2];
                INTERP_SHADETAB->accum[1] = *(const uint8_t*)INTERP_TEXTURE->pop[2];
                p[3] = *(const uint16_t*)INTERP_SHADETAB->pop[2];
                INTERP_SHADETAB->accum[1] = *(const uint8_t*)INTERP_TEXTURE->pop[2];
                p[4] = *(const uint16_t*)INTERP_SHADETAB->pop[2];
                INTERP_SHADETAB->accum[1] = *(const uint8_t*)INTERP_TEXTURE->pop[2];
                p[5] = *(const uint16_t*)INTERP_SHADETAB->pop[2];
                INTERP_SHADETAB->accum[1] = *(const uint8_t*)INTERP_TEXTURE->pop[2];
                p[6] = *(const uint16_t*)INTERP_SHADETAB->pop[2];
                INTERP_SHADETAB->accum[1] = *(const uint8_t*)INTERP_TEXTURE->pop[2];
                p[7] = *(const uint16_t*)INTERP_SHADETAB->pop[2];
#endif

                p += X_RES;
                usy += usdy; uey += uedy;
                vsy += vsdy; vey += vedy;
                lsy += lsdy; ley += ledy;
            } while (--y);
            gr++;
            p -= (X_RES * GRID_SIZE) - GRID_SIZE;
        }
        gr++;
        p += (X_RES * (GRID_SIZE-1));
    } 
}

static const float FOV = 140;
static const float tunnel_size = 48.0f;

void calcgrid(grid_t *grid, vec3f &o, vec3f &d, float t) {
    vec3f origin = o, intersect, direction;
    mat3  rot;
    
    // make rotation matrix
    rot3(rot, d.x, d.y, d.z);
    
    float dist = FOV;
    float size = tunnel_size;

    // adjust dist
    float _1dist = 1.0f / dist;
    float sqsqsize = tunnel_size;
    // precalc
    float  tsq = sqr(sqsqsize);
    float _1pi = 1.0f / pi;
    float qx = sqr(origin.x);
    float qy = sqr(origin.y);
    float qz = sqr(origin.z);
    float aa = tunnel_size*tunnel_size * ((0.2f * cos(t * 0.4)) + 0.2f);
    //float aa = (size * size * 0.6);

    float l0 = 63, l1 = 32;

    if (t < 1.0) {
        l0 = 63;
        l1 = smoothstep(63, 32, clamp(t, 0, 1));
    }

    if (t > 20.0f) {
        float tt = (t - 20.0f)/2.0f;
        l0 = smoothstep(63, 63, clamp(tt, 0, 1));
        l1 = smoothstep(32, 63, clamp(tt, 0, 1));
    }

    grid_t *p = grid;
    for (int y = 0; y < (Y_RES/GRID_SIZE)+1; y++) {
        for (int x = 0; x < (X_RES/GRID_SIZE)+1; x++) {
            float fu, fv;
            
            // set direction vector
            direction.x = (float)((x * GRID_SIZE) - (X_RES/2)) * _1dist;
            direction.z = 1;
            direction.y = (float)((y * GRID_SIZE) - (Y_RES/2)) * _1dist;
            
            // normalize and rotate
            norm(direction);
            direction = mul(direction, rot);
            
            float alt = aa * (cos(pi * (atan2(direction.y, direction.x)) + t));
            
            float a = (sqr(direction.x) + sqr(direction.y));
            float b = 2.0f * (origin.x * direction.x + origin.y * direction.y);
            float c = (qx + qy - tsq - alt);
            
            float delta = sqrt(b * b - 4.0f * a * c);
            float _2a   = 1.0f / (2.0f * a);
            
            float t1 = (-b + delta) * _2a;
            float t2 = (-b - delta) * _2a;

            float t = (t1 > 0.0 ? t1 : t2);
            
            // lerp
            intersect = origin + direction*t;
            
            // get uv
            fu = ((intersect.z) * 65536.0f * 1.0f);
            fv = ((fabs(atan2(intersect.y, intersect.x)) * 128.0f * 65536.0f * _1pi));
            
            // get lighting
            float l = mix(l0, l1, clamp(30.0f / t, 0, 1));
            float fl = l * 65536.0f;

            p->u = (int32_t)fu & 0xFFFFFFFF;
            p->v = (int32_t)fv & 0xFFFFFFFF;
            p->l = (int32_t)fl & 0xFFFFFFFF;

            p++;
        }
    }
}

void tunnel_run() {
    const uint16_t *texture  = (const uint16_t *)texsram;
    const uint16_t *shadetab = (const uint16_t *)shadetabsram;

    fbIdx = 0;
    uint32_t frame_counter = 0;
    
    // init interpolators
    mytmap_interp_setup_uv (INTERP_TEXTURE,  texture,  16, 8, 8, 0);
    mytmap_interp_setup_lsh(INTERP_SHADETAB, shadetab, 16, 6, 8, 1);

    while(lxm_current_frame() < (3*16 + 12*3*64)) {
        float t = ftimer_get();
        // no need to clear buffer

        vec3f o;
        o.x = tunnel_size*0.3f*sin(t*1.6f) + tunnel_size*(0.5f * sin(t*1.73f));
        o.y = tunnel_size*0.3f*cos(t*1.78f);
        o.z = t * tunnel_size * 3.2f ;

        vec3f dir;
        dir.x = pi/4 + t*0.24f + 1.67f*cos(t*0.6f) + 0.45f*sin(t*0.9f);
        dir.y = pi/2 + t*0.20f + 1.50f*sin(t*0.6f) + 0.41f*cos(t*0.9f);
        dir.z = pi/4 + 0.9f*sin(t*0.8f);

        calcgrid(grid, o, dir, t);
        drawgrid(fb[fbIdx], grid, texture);
        fb_blend_buf_a(&fb[fbIdx], &fb[fbIdx^1], X_RES*Y_RES);

        // draw rasterdot and flip buffers
        dvi_set_framebuffer(&fb[fbIdx], 0); fbIdx ^= 1;
        dvi_wait_for_vblank();
        frame_counter++;
    }
}
