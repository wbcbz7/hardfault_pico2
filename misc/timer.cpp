#include "pico/stdlib.h"
#include <timer.h>

// start time stamp
static uint32_t ftimer_start_us;
static float    ftimer_start_advance;

void ftimer_set(float time) {
    ftimer_start_us      = time_us_32();
}

float ftimer_get() {
    return ((time_us_32() - ftimer_start_us) / 1000000.0f) + ftimer_start_advance;
}
