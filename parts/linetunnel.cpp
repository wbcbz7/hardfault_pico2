#include "linetunnel.h"
#include <stdint.h>
#include <vec.h>
#include <dvi.h>
#include <fbstuff.h>
#include <defs.h>
#include <argb.h>

// отрезать отсюда и в linedraw.cpp
// -----------------------

#define vcode(p) (((p->x < 0) ? 1 : 0) | ((p->x >= X_RES) ? 2 : 0) | ((p->y < 0) ? 4 : 0) | ((p->y >= Y_RES) ? 8 : 0))    

// returns 0 if fully in, -1 if fully out (rejected), else bit 0 - a clipped, bit 1 - b clipped, clips in place!
int lineclip(vec2f *a, vec2f *b) {
    int code_a, code_b, code, rtn = 0;;
    vec2f *c;
 
    code_a = vcode(a);
    code_b = vcode(b);
 
    while (code_a || code_b) {
        if (code_a & code_b)
            return -1;
 
        if (code_a) {
            code = code_a;
            c = a;
            rtn |= 1;
        } else {
            code = code_b;
            c = b;
            rtn |= 2;
        }
 
        if (code & 1) {
            c->y += (a->y - b->y) * (0 - c->x) / (a->x - b->x + ee);
            c->x = 0;
        } else if (code & 2) {
            c->y += (a->y - b->y) * (X_RES - c->x) / (a->x - b->x + ee);
            c->x = X_RES - 1;
        }

        if (code & 4) {
            c->x += (a->x - b->x) * (0 - c->y) / (a->y - b->y + ee);
            c->y = 0;
        } else if (code & 8) {
            c->x += (a->x - b->x) * (Y_RES - c->y) / (a->y - b->y + ee);
            c->y = Y_RES - 1;
        }
 
        if (code == code_a)
            code_a = vcode(a);
        else
            code_b = vcode(b);
    }
 
    return rtn;
}

// 0.0 < abs(tan(y/x)) < 1.0
void drawline_x(uint16_t *dst, int x1, int y1, int x2, int y2, int color) {
    if (x1 > x2) {
        int tmp = x2; x2 = x1; x1 = tmp;
        tmp = y2; y2 = y1; y1 = tmp;
    }
    int dydx = idiv16((y2-y1),(x2-x1));
    // prestep
    int y = y1 + imul16(((-x1)&0xFFFF), dydx);
    // draw line
    for (int x = ceilx(x1); x < ceilx(x2); x++) {
        *(dst + X_RES * (y >> 16) + x) = color;
        y += dydx;
    }
}

// abs(tan(y/x)) > 1.0
void drawline_y(uint16_t *dst, int x1, int y1, int x2, int y2, int color) {
    if (y1 > y2) {
        int tmp = x2; x2 = x1; x1 = tmp;
        tmp = y2; y2 = y1; y1 = tmp;
    }
    int dxdy = idiv16((x2-x1),(y2-y1));
    // prestep
    int x = x1 + imul16(((-y1)&0xFFFF), dxdy);
    // draw line
    for (int y = ceilx(y1); y < ceilx(y2); y++) {
        *(dst + (X_RES * y) + (x >> 16)) = color;
        x += dxdy;
    }
}

void drawline_h(uint16_t *dst, int x1, int y1, int x2, int dx, int color) {
    uint16_t *buf = dst + (X_RES * ceilx(y1)) + ceilx(x1 > x2 ? x2 : x1);
    for (int i = 0; i < dx; i++) {*buf++ = color;}
}

void drawline_v(uint16_t *dst, int x1, int y1, int y2, int dy, int color) {
    uint16_t *buf = dst + (X_RES * ceilx(y1 > y2 ? y2 : y1)) + ceilx(x1);
    for (int i = 0; i < dy; i++) {*buf = color; buf += X_RES;}
}

// not a bresenham :]
void drawline_subpixel(uint16_t *dst, int x1, int y1, int x2, int y2, int color) {
    int dx = ceilx(x2) - ceilx(x1);
    int dy = ceilx(y2) - ceilx(y1);
    if (dx == 0 && dy == 0) return;     // nothing to draw!
    if (dy == 0) return drawline_h(dst, x1, y1, x2, abs(dx), color);
    if (dx == 0) return drawline_v(dst, x1, y1, y2, abs(dy), color);
    
    if (abs(dy) > abs(dx)) {
        return drawline_y(dst, x1, y1, x2, y2, color);
    } else {
        return drawline_x(dst, x1, y1, x2, y2, color);
    }
};

// и до сюда
// ----------------------

enum {
    TOTAL_LINES = 5,
    SEGS_PER_FRAME = 20,
};

void proj(vec2f &out, vec3f &in) {
    const float fov = 80.0f;
    out.x = ((fov * in.x) / (in.z + 1e-4)) + (X_RES/2);
    out.y = ((fov * in.y) / (in.z + 1e-4)) + (Y_RES/2);
}

// -----------------------

void linetunnel_init()
{
}

void linetunnel_run()
{
    fbIdx = 0;
    uint32_t frame_counter = 0;

    // init palette
    uint16_t linepal[16];
    argb32 bgcolor;   bgcolor.r   = 0x18, bgcolor.g   = 0x18, bgcolor.b   = 0x60;
    argb32 linecolor; linecolor.r = 0xD0, linecolor.g = 0xD0, linecolor.b = 0xFF;
    uint32_t bgcolor16 = ((argb_to_555(bgcolor) | (argb_to_555(bgcolor) << 16)) >> 1) & 0x3DEF3DEF;
    {
        argb32 cc;
        for (int i = 0; i < 16; i++) {
            cc.r = ((bgcolor.r * (15-i)) + (linecolor.r * i)) >> 4;
            cc.g = ((bgcolor.g * (15-i)) + (linecolor.g * i)) >> 4;
            cc.b = ((bgcolor.b * (15-i)) + (linecolor.b * i)) >> 4;
            linepal[i] = argb_to_555(cc);
        }
    }

    int total_segs = SEGS_PER_FRAME;
    while(1) {
        fb_blend_const(&fb[fbIdx], bgcolor16, X_RES*Y_RES);

        const float SEG_DIST = 32.0;
        float fpos = frame_counter * 4;

        int fseg = fpos / SEG_DIST;
        int min_seg = max(20 - fseg, 0);
        
#if 0
        if ((frame_counter >= 400) && ((frame_counter&7) == 0)) {
            total_segs--;
        }
#endif

        for (int seg = total_segs-1; seg >= min_seg; seg--) {
            vec2f segofs;
            segofs.x = sin((seg + fseg) * 0.3 + frame_counter * 0.04) * SEG_DIST * 1;
            segofs.y = cos((seg + fseg) * 0.2 + frame_counter * 0.04) * SEG_DIST * 1;
            float zz = fmod((-fpos), SEG_DIST) + (SEG_DIST * seg) + SEG_DIST + 10;
            float length = 2 * pi * ((float)1 / TOTAL_LINES) * (1.0 + 0.4 * sin(frame_counter * 0.03));
            float aa_seg_ofs = (frame_counter * 0.001) + 0.4*sin((fseg + seg) * 0.2 + frame_counter * 0.01);
            int col = min(15, 1250 / zz);

            //segofs.x -= sin(((float)fpos/SEG_DIST) * 0.3 + frame_counter * 0.04) * 60.0;
            //segofs.y -= cos(((float)fpos/SEG_DIST) * 0.2 + frame_counter * 0.04) * 60.0;

            for (int i = 0; i < TOTAL_LINES; i++) {
                float r = 120;
                float a[2];
                float aa = 2 * pi * ((((float)i / TOTAL_LINES)) + aa_seg_ofs);
                a[0] = aa - (length * 0.5);
                a[1] = aa + (length * 0.5);

                vec3f p[2];
                p[0].x = r * cos(a[0]) + segofs.x;
                p[0].y = r * sin(a[0]) + segofs.y;
                p[0].z = zz;

                p[1].x = r * cos(a[1]) + segofs.x;
                p[1].y = r * sin(a[1]) + segofs.y;
                p[1].z = zz;

                vec2f pt[2];
                proj(pt[0], p[0]);
                proj(pt[1], p[1]);

                if (lineclip(&pt[0], &pt[1]) != -1) drawline_subpixel(
                    fb[fbIdx],
                    pt[0].x*65536.0f,
                    pt[0].y*65536.0f,
                    pt[1].x*65536.0f,
                    pt[1].y*65536.0f,
                    linepal[col]
                );
            }
        }

        dvi_set_framebuffer(&fb[fbIdx], 0); fbIdx ^= 1;
        dvi_wait_for_vblank();
        frame_counter++;
    }
}

void linetunnel_done()
{
}
