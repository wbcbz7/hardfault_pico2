#pragma once
#include <stdint.h>

typedef union argb32 {
    struct {
        uint8_t b, g, r, a;
    };
    uint32_t v;
} argb32;

// convert to RGB565
inline uint32_t argb_to_555(argb32 &a) {
    return ((a.b & 0xF8) >> 3) | ((a.g & 0xF8) << 2) | ((a.r & 0xF8) << 7);
}

// convert to RGB555

