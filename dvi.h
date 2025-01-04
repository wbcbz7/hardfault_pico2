#pragma once
#include <stdint.h>
#include <stdlib.h>
#include "hardware/pio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    timing chart:

    ------hhhh------------------------------------------    ] vertical back porch
    ------hhhh------------------------------------------    ] 
    vvvvvvHVHVvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv    ] vertical sync 
    vvvvvvHVHVvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv     ]
    ------HHHH------------------------------------------    ] vertical front porch (w/ optional HDMI data islands)
    ------HHHH------------------------------------------    ]
    ------HHHH-----bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb    ] vertical border (optional)
    ------HHHH-----AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    ------HHHH-----AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    ------HHHH-----AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    ------HHHH-----AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ] active area
    ------HHHH-----AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    ------HHHH-----AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    ------HHHH-----AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    ------HHHH-----AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    ------HHHH-----bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb    ] vertical border (optional)
     ^     ^    ^     active or vertical border area
     ^     ^    front porch
     ^     sync
     back porch
*/

// state machine phases
enum {
    DVI_STATE_IDLE = 0,
    DVI_STATE_FRONT_PORCH,
    DVI_STATE_SYNC,
    DVI_STATE_BACK_PORCH,       // TODO: HDMI stuff with data packets
    DVI_STATE_TOP_BORDER,
    DVI_STATE_ACTIVE_BLANK,
    DVI_STATE_ACTIVE_PIXELS,
    DVI_STATE_BOTTOM_BORDER,
};

// pixel formats
enum {
    DVI_PIXEL_FORMAT_NULL = 0,
    DVI_PIXEL_FORMAT_XRGB8888,      // 1px/word
    DVI_PIXEL_FORMAT_RGB565,        // 2px/word
    DVI_PIXEL_FORMAT_XRGB1555,      // 2px/word
    DVI_PIXEL_FORMAT_XRGB4444,      // 2px/word
    DVI_PIXEL_FORMAT_RGB332,        // 4px/word
    DVI_PIXEL_FORMAT_XRGB2222,      // 4px/word
    DVI_PIXEL_FORMAT_GRAY8,         // 4px/word, grayscale
    DVI_PIXEL_FORMAT_DIRECT_END,

    // palette index formats here
    DVI_PIXEL_FORMAT_INDEX_RANGE = (1 << 8),
    DVI_PIXEL_FORMAT_INDEX8 = DVI_PIXEL_FORMAT_INDEX_RANGE, // 4px/word, palette
};

// same formats (except for INDEX8 of course :) can be used for palette CLUT

// pixel replication formats
enum {
    DVI_PIXEL_REP_1,                        // single pixels
    DVI_PIXEL_REP_2,                        // doubled pixels
    DVI_PIXEL_REP_4,                        // quadrupled pixels
    DVI_PIXEL_REP_MASK       = (3 << 0),    // pixel replication settings mask

    DVI_PIXEL_SMOOTH_PANNING = (1 << 8),    // require smooth panning suport
};

/*
    pixel replication scheme
    direct color/grayscale:
        single:
            SRAM --w--> HSTX
            pixels_per_word/bytes_per_pixels as is

            or, if smooth scrolling is required:
            SRAM --w--> PIO --w--> HSTX

        double:
            1px/word:
            SRAM --w--> PIO --w--> HSTX, PIO does DCBA -> BBAA DDCC replication
            2px/word:
            SRAM --h--> HSTX, replication in HSTX command expander
            4px/word:
            SRAM --w--> HSTX, replication in HSTX command expander

        quad:
            SRAM -bhw-> HSTX, data size by bytes_per_pixel, replication in HSTX command expander

    palette:
            SRAM --w--> PIO --w--> DMA -bhw-> HSTX
            PIO outputs palette address, which is used as trigger for DMA write to the HSTX
            replication in HSTX command expander

            UPD: seems to not work as expected :(
*/

enum {
    DVI_XFER_MODE_SRAM_HSTX,
    DVI_XFER_MODE_SRAM_PIO_HSTX,
    DVI_XFER_MODE_SRAM_PIO_DMA_HSTX,

    DVI_XFER_MODE_END
};

// HSTX expander layout
union dvi_hstx_expand_layout_t {
    struct {
        uint8_t rot  :5;
        uint8_t nbits:3;
    } lane[3];
    uint32_t raw;
};

// HSTX input mode
struct dvi_hstx_input_params_t {
    uint8_t  pixels_per_word;
    uint8_t  bits_per_pixel;
    uint8_t  rep_count;         // pixel replication count in HSTX command expander
    uint8_t  reserved;

    // expand layout
    union dvi_hstx_expand_layout_t expand_tmds;
};

// HSTX pin layout struct, pin numbers relative to GP12
union dvi_hstx_pin_layout_t {
    struct {
        uint32_t clock_n : 4;
        uint32_t clock_p : 4;
        uint32_t lane0_n : 4;
        uint32_t lane0_p : 4;
        uint32_t lane1_n : 4;
        uint32_t lane1_p : 4;
        uint32_t lane2_n : 4;
        uint32_t lane2_p : 4;
    };
    uint32_t raw;
};

// DVI/HDMI timing struct
struct dvi_timings_t {
    // length of each frame part
    struct {
        int16_t front_porch;
        int16_t sync;
        int16_t back_porch;
        int16_t active;
        int16_t total;
    } h;

    struct {
        int16_t front_porch;
        int16_t sync;
        int16_t back_porch;
        int16_t border_top;
        int16_t active;
        int16_t border_bottom;
        int16_t total;

        int32_t refresh;        // in ([hz]*1000), 0 if calculate from current hstx_clk + timing parameters
    } v;

    int32_t  pixelclock;        // requested pixel clock in Hz, 0 if use (current_hstx_clk/5)
    uint32_t flags;             // polarity etc
};

enum {
    // sync polarity flags
    DVI_TIMINGS_H_POS = (0 << 0),
    DVI_TIMINGS_H_NEG = (1 << 0),
    DVI_TIMINGS_V_POS = (0 << 1),
    DVI_TIMINGS_V_NEG = (1 << 1),
    DVI_TIMINGS_SYNC_POLARITY_MASK = (3 << 0),
};

enum {
    DVI_SET_TIMINGS_REFRESH_VBLANK = (0 << 0),  // stretch vblank to change refresh rate
    DVI_SET_TIMINGS_REFRESH_HBLANK = (1 << 0),  // stretch hblank instead (more compatible with HDMI->VGA?)
};

// bus error info struct
struct dvi_bus_error_t {
    volatile int    num;        // number of offending DMA channel (-1 if none)
    int             state;      // DVI state at the moment of bus error
    uint32_t        scanline;   // at which scanline bus error occured
    struct {
        // saved registers
        uint32_t read_addr, write_addr, transfer_count, ctrl;
    } regs;
};

// ----------------------------
// FUNCTIONS
// sorry for no doxygen at this moment - l8r =)

// init DVI context. do NOT stops current video output
// usually called at application init
void dvi_ctx_init();

// TODO: setting clocks

// set display timings 
// input:
//    t - DVI timings definitions struct 
//      fill front/back porch, sync and active fields
//      v.refresh defines "desired" refresh in [hz]*1000 (must be less than mode refresh for current pixelclock!)
//      v.refresh==0 - use current refresh
//    flags - additional flags:
//      DVI_SET_TIMINGS_REFRESH_VBLANK/DVI_SET_TIMINGS_REFRESH_HBLANK
//          if v.refresh is lower than actual refresh rate, decide wheter to stretch horizontal or vertical
//          blanking interval to match requested refresh rate
//          vertical (default) increases total vertical lines per frame and leaves horizontal rate intact
//          horizontal can be more compatible with HDMI->VGA converters and VGA monitors but decreases horizontal scan rate
int dvi_set_timings(struct dvi_timings_t *t, int flags);

// set line repeat (default is 1)
void dvi_set_line_repeat(int repeat);

struct dvi_resources_t {
    int     dma_channels_num : 7;
    int     pio_required : 1;

    int     xfer_mode;          // transfer mode
    int     hstx_dma_size;      // speaks for itself
    int     dma_channels[4];    // free DMA channel munvers
    PIO     pio;                // PIO handle
    int     sm;                 // free state machine for given PIO, or -1 if find free in dvi_configure_pio()
    int     dma_irq_line;       // exclusive(!) DMA IRQ line
};

/*  get resource requirements for pixel format/repeat count
    input:
        src_format - pixel format
        pal_format - palette CLUT format (for indexed src_format, ignored otherwise)
        flags      - pixel replication mode (1x, 2x, 4x) and smooth scrolling flag
        res        - structure with resources required
    output:
        0 if ok + res filled with info, 1 if format not supported
*/
int dvi_get_resources_required(int src_format, int pal_format, int flags, struct dvi_resources_t *res);

/*  claim PIO and configure a state machine
    input:
        src_format - pixel format enum
        pal_format - palette CLUT format (for indexed src_format, ignored otherwise)
        flags      - pixel replication mode (1x, 2x, 4x) and smooth scrolling flag
        res        - structure with resources required
    output:
        0 if ok + PIO configured, 1 if failed
    notes:
        PIO _must_ be clocked from pll_sys, otherwise it may not work fast enough to supply pixel data to HSTX!
*/
int dvi_configure_pio(int src_format, int pal_format, int flags, struct dvi_resources_t *res);

/*  configure HSTX input
    input:
        format - HSTX pixel format enum (or CLUT format for palette)
        flags  - pixel replication mode (1x, 2x, 4x) and smooth scrolling flag
        ptr    - pointer to custom format if (format==DVI_PIXEL_FORMAT_CUSTOM), otherwise 0
*/
int dvi_configure_hstx_input(int format, int flags, int xfer_mode, const struct dvi_hstx_input_params_t *ptr);

// set active pixels transfer mode
int dvi_configure_xfer_mode(int mode);

// configure HSTX output
// input:
//   layout - struct defining HSTX pin mapping to TMDS lanes
void dvi_configure_hstx_output(union dvi_hstx_pin_layout_t layout);

/*  init resources for the display output
    input:
        res        - structure with resources required
    output:
        0 if ok, non-0 if error
*/
int dvi_init_dma(const struct dvi_resources_t* res);

// start DMA display output and reset scanline counter
int dvi_start_dma();

// stop DMA display output
void dvi_stop_dma();

/*  set frame buffer address and pitch
    input:
        ptr         - pointer to frame buffer start
        pitch       - distance between two adjacent scanlines in bytes
        immediate   - false if update at next vblank, true if immediately (may cause screen tearing)
*/
void dvi_set_framebuffer(void *ptr, int immediate);
void dvi_set_pitch(uint32_t pitch, int immediate);

// wait for vblank or hblank
void dvi_wait_for_vblank();
void dvi_wait_for_hblank();

// set current offset in framebuffer (WILL cause tearing, use only with careful beam racing!)
void dvi_set_offset(uint32_t offset);

// set pixel panning offset
void dvi_set_pixel_panning(uint32_t offset, int immediate);

// check if bus error occured
int dvi_is_bus_error();

// get bus error struct
volatile struct dvi_bus_error_t* dvi_get_bus_error_info();

// get current video output state
int dvi_get_current_state();

// get current scanline and frame, leading by 2 scanlines due to DMA sequencing logic
uint32_t dvi_get_frame_count();
uint32_t dvi_get_current_scanline();
int32_t dvi_get_current_active_scanline();

#ifdef __cplusplus
}
#endif
