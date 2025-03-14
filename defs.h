#pragma once
#include <stdint.h>

#define X_RES 320
#define Y_RES 240

#define BYTES_PER_PIXEL 2
#define X_PITCH (X_RES*BYTES_PER_PIXEL)

#define X_RES_SCROLLER   352
#define X_PITCH_SCROLLER (X_RES_SCROLLER*BYTES_PER_PIXEL)

// stability test stuff
//#define TEXTURES_FROM_FLASH
//#define USE_DEFAULT_SYSTEM_CLOCK
#define DO_OVERVOLT
#define OVERVOLT_VOLTAGE VREG_VOLTAGE_1_40

// clock multiplier
#ifndef CLK_SYS_MUL
#define CLK_SYS_MUL          2
#endif

// video mode pixel clock
#define MODE_PIXEL_CLOCK     25200*KHZ

// board define (TODO: do this via CMake)
#define HSTX_OUT_MURMULATOR2
//#define HSTX_OUT_PICODVISOCK

#ifdef PICO_BUILD
#include "pico.h"
#define __unroll __attribute__((optimize("unroll-loops")))
#else
#define __not_in_flash_func(x) x
#define __scratch_x(x) 
#define __scratch_y(x) 
#define __unroll 
#endif

// -------------------------
// SHARED RESOURCES (WARNING: source of potential bugs :D)

// frame buffer
extern uint16_t fb[2][X_RES * Y_RES];
extern uint8_t  fbIdx;

// core 1 "beacon"
extern uint32_t core1_beacon;
