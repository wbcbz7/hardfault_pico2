#include "pico/stdlib.h"
#include <timer.h>
#include <dvi.h>
#include <defs.h>

// start time stamp
static uint32_t ftimer_start_us;
static float    ftimer_start_advance;

void ftimer_set(float time) {
    ftimer_start_us      = time_us_32();
}

float ftimer_get() {
    return ((time_us_32() - ftimer_start_us) / 1000000.0f) + ftimer_start_advance;
}

// display raster dot of given color
void rasterdot(uint32_t color) {
    int32_t scanline = dvi_get_current_active_scanline() / 2;
    if (scanline >= 0 && scanline < Y_RES-1) {
        fb[fbIdx^1][(scanline+1)*X_RES] = color;
    }
}

void rasterdot_xor(uint32_t color) {
    int32_t scanline = dvi_get_current_active_scanline() / 2;
    if (scanline >= 0 && scanline < Y_RES-1) {
        fb[fbIdx^1][(scanline+1)*X_RES] ^= color;
    }
}