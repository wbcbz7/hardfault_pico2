#pragma once 
#include <stdint.h>

// set timer value
void ftimer_set(float time);

// get timer value
float ftimer_get();

// display raster dot of given color
void rasterdot(uint32_t color);
void rasterdot_xor(uint32_t color);
