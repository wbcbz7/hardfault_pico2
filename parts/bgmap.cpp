#include "bgmap.h"
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

#include "../textures/bgmap_tex.h"
#include "../textures/bgmap_tiles.h"

#define MAX_TILES   10
#define MAX_TILES   10
#define GRID_SIZE   8
#define X_GRID      (X_RES/GRID_SIZE)
#define Y_GRID      (Y_RES/GRID_SIZE)
#define X_RES_GRID  (X_GRID*GRID_SIZE)
#define Y_RES_GRID  (Y_GRID*GRID_SIZE)

#define TEXTURE_SIZE_LOG2 5
#define TEXTURE_SIZE (1 << TEXTURE_SIZE_LOG2)

// tilemap texture
uint8_t  *texture;//[TEXTURE_SIZE*TEXTURE_SIZE];

// mapping table
uint16_t *mappingtab;//[X_GRID*Y_GRID];

// tile graphics
uint16_t *tilegfx;//[MAX_TILES][GRID_SIZE*GRID_SIZE];

static void calctab();
static void calctiles();
static void calctex();

void bgmap_init() {
    texture    = new uint8_t[TEXTURE_SIZE*TEXTURE_SIZE];
    mappingtab = new uint16_t[X_GRID*Y_GRID];
    tilegfx    = new uint16_t[MAX_TILES*GRID_SIZE*GRID_SIZE];

    calctex();
    calctiles();
    calctab();
}

void bgmap_done() {
    delete[] texture;
    delete[] mappingtab;
    delete[] tilegfx;
}

static void calctab() {
    uint16_t *t = mappingtab;
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

static void calctex() {
    uint8_t *t = texture;
    for (int y = -TEXTURE_SIZE/2; y < TEXTURE_SIZE/2; y++) {
        for (int x = -TEXTURE_SIZE/2; x < TEXTURE_SIZE/2; x++) {
            //int r = (x ^ y) & 7;
            //*t++ = r;
            float r = sin(sqrt(x*x + y*y)*0.2);
            *t++ = ((r * 4.5) + 5);
        }
    }
}

static void calctiles() {
    const uint8_t *p = bgmap_tiles;
    for (int gl = 0; gl < MAX_TILES; gl++) {
        uint16_t *tile = tilegfx + (gl*GRID_SIZE*GRID_SIZE);
        for (int y = 0; y < GRID_SIZE; y++) {
            for (int x = 0; x < GRID_SIZE; x++) {
                uint8_t a = *p++;
                *tile++ = a > 0 ? argb_to_555(32, 80, 64) : argb_to_555(24, 32, 24);
            }
        }
    }
}

extern "C" void bgmap_copy_8x8(uint16_t *dst, uint16_t *src, uint32_t dstfixup);

static void map(uint16_t *fb, uint32_t tabofs) {
    uint32_t texmask = (1 << (TEXTURE_SIZE_LOG2*2)) - 1;
    uint16_t *p = fb;
    uint16_t *tab = mappingtab;
    for (int j = 0; j < Y_GRID; j++) {
        for (int i = 0; i < X_GRID; i++) {
            uint16_t *tile = &tilegfx[bgmap_texture[(*tab++ + tabofs) & texmask]*GRID_SIZE*GRID_SIZE];

#if 0
            for (int y = 0; y < GRID_SIZE; y++) {
                for (int x = 0; x < GRID_SIZE; x++) {
                    *p++ = *tile++;
                }
                p += X_RES - GRID_SIZE;
            }
            p -= (X_RES * GRID_SIZE) - GRID_SIZE;
#else
            bgmap_copy_8x8(p, tile, (X_RES - GRID_SIZE)*BYTES_PER_PIXEL);
            p += GRID_SIZE;
#endif
        }
        p += (X_RES * (GRID_SIZE-1));
    }
}

void bgmap_run() {
    fbIdx = 0;
    uint32_t frame_counter = 0;

    while(1) {
        float t = frame_counter / 60.0f;
        
        {
            int u = TEXTURE_SIZE*t*1.0f;
            int v = TEXTURE_SIZE*(1.5f*sin(t*1.0f) + cos(t*0.8f));
            uint32_t tabofs = (u & (TEXTURE_SIZE-1)) | ((v & (TEXTURE_SIZE-1)) << TEXTURE_SIZE_LOG2);
            map(fb[fbIdx], tabofs);
        }

        // fraw rasterdot and flip buffers
        rasterdot_xor(argb_to_555(255, 255, 255));
        dvi_set_framebuffer(&fb[fbIdx], 0); fbIdx ^= 1;
        dvi_wait_for_vblank();
        frame_counter++;
    }
}