#include <linedraw.h>
#include <stdint.h>
#include <defs.h>

// отрезать отсюда и в linedraw.cpp
// -----------------------

#define vcode(p) (((p->x < 0) ? 1 : 0) | ((p->x >= X_RES) ? 2 : 0) | ((p->y < 0) ? 4 : 0) | ((p->y >= Y_RES) ? 8 : 0))    

// returns 0 if fully in, -1 if fully out (rejected), else bit 0 - a clipped, bit 1 - b clipped, clips in place!
int lineclip(vec2f *a, vec2f *b, vec4f *bbox) {
    int code_a, code_b, code, rtn = 0;
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
            c->y += (a->y - b->y) * (bbox->x - c->x) / (a->x - b->x + ee);
            c->x = 0;
        } else if (code & 2) {
            c->y += (a->y - b->y) * (bbox->z - c->x) / (a->x - b->x + ee);
            c->x = bbox->z;
        }

        if (code & 4) {
            c->x += (a->x - b->x) * (bbox->y - c->y) / (a->y - b->y + ee);
            c->y = 0;
        } else if (code & 8) {
            c->x += (a->x - b->x) * (bbox->w - c->y) / (a->y - b->y + ee);
            c->y = bbox->w;
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
