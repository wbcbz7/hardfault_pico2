#include "dvi.h"
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/structs/sio.h"

#include "dvi.pio.h"

// DVI/HDMI constants

#define TMDS_CTRL_00 0x354u
#define TMDS_CTRL_01 0x0abu
#define TMDS_CTRL_10 0x154u
#define TMDS_CTRL_11 0x2abu

#define SYNC_V0_H0 (TMDS_CTRL_00 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V0_H1 (TMDS_CTRL_01 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H0 (TMDS_CTRL_10 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H1 (TMDS_CTRL_11 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))

#define HDMI_PREAMBLE_WIDTH  8
#define HDMI_GUARDBAND_WIDTH 2

#define HDMI_VIDEO_PREAMBLE(sync_symbol) ((sync_symbol & 0x3FF) | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_00 << 20))
#define HDMI_DATA_PREAMBLE (sync_symbol) ((sync_symbol & 0x3FF) | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_01 << 20))

#define HDMI_VIDEO_GUARDBAND (0x2ccu | (0x133u << 10) | (0x2ccu << 20))
#define HDMI_DATA_GUARDBAND(sync_code) (hdmi_terc4_table[12 + sync_code] | (0x133u << 10) | (0x133u << 20))

#define HSTX_CMD_RAW         (0x0u << 12)
#define HSTX_CMD_RAW_REPEAT  (0x1u << 12)
#define HSTX_CMD_TMDS        (0x2u << 12)
#define HSTX_CMD_TMDS_REPEAT (0x3u << 12)
#define HSTX_CMD_NOP         (0xfu << 12)

// helper struct for *_REPEAT HSTX modes
union hstx_token_t {
    struct {
        uint32_t length : 12;
        uint32_t type   : 4;
    };
    uint32_t raw;
};

struct hstx_repeat_token_t {
    union hstx_token_t  token;
    uint32_t            data;
};

// ------------------------
// line scripts
// nops are inserted to pad list to HSTX FIFO length (8 words)

// blanking line
struct hstx_dvi_blank_script_t {
    struct hstx_repeat_token_t front_porch;
    union  hstx_token_t        nop0;
    struct hstx_repeat_token_t sync;
    union  hstx_token_t        nop1;
    struct hstx_repeat_token_t back_porch;
    union  hstx_token_t        nop2;
};

// border line
struct hstx_dvi_border_script_t {
    struct hstx_repeat_token_t front_porch;
    union  hstx_token_t        nop0;
    struct hstx_repeat_token_t sync;
    union  hstx_token_t        nop1;
    struct hstx_repeat_token_t back_porch;
    struct hstx_repeat_token_t border;
};

// active area
struct hstx_dvi_active_script_t {
    struct hstx_repeat_token_t front_porch;
    union  hstx_token_t        nop0;
    struct hstx_repeat_token_t sync;
    union  hstx_token_t        nop1;
    struct hstx_repeat_token_t back_porch;
    union  hstx_token_t        nop2;
    union  hstx_token_t        active;
};

// border line for HDMI
struct hstx_hdmi_border_script_t {
    struct hstx_repeat_token_t front_porch;
    union  hstx_token_t        nop0;
    struct hstx_repeat_token_t sync;
    union  hstx_token_t        nop1;
    struct hstx_repeat_token_t back_porch;
    struct hstx_repeat_token_t preamble;
    struct hstx_repeat_token_t guardband;
    struct hstx_repeat_token_t border;
};

// active area for HDMI
struct hstx_hdmi_active_script_t {
    struct hstx_repeat_token_t front_porch;
    union  hstx_token_t        nop0;
    struct hstx_repeat_token_t sync;
    union  hstx_token_t        nop1;
    struct hstx_repeat_token_t back_porch;
    union  hstx_token_t        nop2;
    struct hstx_repeat_token_t preamble;
    struct hstx_repeat_token_t guardband;
    union  hstx_token_t        active;
};

// ----------------------------------------------------------------------------
// HSTX command lists

static struct hstx_dvi_blank_script_t hstx_script_blank = {
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V1_H1 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V1_H0 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V1_H1 },
    { .raw  = HSTX_CMD_NOP }
};
static struct hstx_dvi_blank_script_t hstx_script_sync = {
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V0_H1 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V0_H0 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V0_H1 },
    { .raw  = HSTX_CMD_NOP }
};
static struct hstx_dvi_border_script_t hstx_script_border = {
    {{.raw  = HSTX_CMD_RAW_REPEAT  | 0},
      .data = SYNC_V1_H1 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT  | 0},
      .data = SYNC_V1_H0 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT  | 0},
      .data = SYNC_V1_H1 },
    {{.raw  = HSTX_CMD_TMDS_REPEAT | 0},
      .data = 0xFFFFFFFF },     // fully white in RGB332
};
static struct hstx_dvi_active_script_t hstx_script_active = {
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V1_H1 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V1_H0 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V1_H1 },
    { .raw  = HSTX_CMD_NOP },
    { .raw  = HSTX_CMD_TMDS       | 0 }
};
static struct hstx_hdmi_border_script_t hstx_script_border_hdmi = {
    {{.raw  = HSTX_CMD_RAW_REPEAT  | 0},
      .data = SYNC_V1_H1 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT  | 0},
      .data = SYNC_V1_H0 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT  | 0},
      .data = SYNC_V1_H1 },
    {{.raw  = HSTX_CMD_RAW_REPEAT  | 0},
      .data = HDMI_VIDEO_PREAMBLE(SYNC_V1_H1) },
    {{.raw  = HSTX_CMD_RAW_REPEAT  | 0},
      .data = HDMI_VIDEO_GUARDBAND },
    {{.raw  = HSTX_CMD_TMDS_REPEAT | 0},
      .data = 0xFFFFFFFF },     // fully white in RGB332
};
static struct hstx_hdmi_active_script_t hstx_script_active_hdmi = {
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V1_H1 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V1_H0 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT | 0},
      .data = SYNC_V1_H1 },
    { .raw  = HSTX_CMD_NOP },
    {{.raw  = HSTX_CMD_RAW_REPEAT  | 0},
      .data = HDMI_VIDEO_PREAMBLE(SYNC_V1_H1) },
    {{.raw  = HSTX_CMD_RAW_REPEAT  | 0},
      .data = HDMI_VIDEO_GUARDBAND },
    { .raw  = HSTX_CMD_TMDS       | 0 }
};

// HDMI TERC4 coding
static const uint32_t hdmi_terc4_table[16] = {
    0x29c,
    0x263,
    0x2e4,
    0x2e2,
    0x171,
    0x11e,
    0x18e,
    0x13c,
    0x2cc,
    0x139,
    0x19c,
    0x2c6,
    0x28e,
    0x271,
    0x163,
    0x2c3
};

// DVI control symbol table
static const uint32_t dvi_ctrl_table[4] = {
    TMDS_CTRL_00, TMDS_CTRL_01, TMDS_CTRL_10, TMDS_CTRL_11
};
static const uint32_t dvi_sync_table[4] = {
    SYNC_V0_H0, SYNC_V0_H1, SYNC_V1_H0, SYNC_V1_H1
};

// ---------------------------

// DMA circular command list
enum {
    COMMAND_LIST_SIZE_LOG2  = 1,
    COMMAND_LIST_SIZE       = (1 << COMMAND_LIST_SIZE_LOG2),    // per half-buffer
    COMMAND_LIST_TOTAL_LOG2 = (COMMAND_LIST_SIZE_LOG2 + 1),
    COMMAND_LIST_TOTAL      = (1 << COMMAND_LIST_TOTAL_LOG2),
};

struct dvi_dma_command_list_t {
    uint32_t    read_addr;
    uint32_t    write_addr;
    uint32_t    trans_count;
    uint32_t    ctrl_trig;
};

// command list storage
static struct dvi_dma_command_list_t dvi_command_list[COMMAND_LIST_TOTAL];

// reset value for the command list, aligned to power-of-two
static __attribute__ ((aligned (8))) struct dvi_dma_command_list_t* dma_command_list_read_addr[2] = {
    &dvi_command_list[COMMAND_LIST_SIZE*1],
    &dvi_command_list[COMMAND_LIST_SIZE*0]
};

// ---------------------------
// HSTX pixel format table

static const struct dvi_hstx_input_params_t dvi_hstx_pixel_formats[] = {
    // DVI_PIXEL_FORMAT_NULL
    {0},
    // DVI_PIXEL_FORMAT_XRGB8888
    {
        .pixels_per_word = 1, .bits_per_pixel = 0, // aka 32
        .expand_tmds = {
            .lane = {
                { .rot =  0, .nbits = 8-1 }, // blue
                { .rot =  8, .nbits = 8-1 }, // green
                { .rot = 16, .nbits = 8-1 }, // red
            }
        }
    },
    // DVI_PIXEL_FORMAT_RGB565
    {
        .pixels_per_word = 2, .bits_per_pixel = 16,
        .expand_tmds = {
            .lane = {
                { .rot = 29, .nbits = 5-1 }, // blue
                { .rot =  3, .nbits = 6-1 }, // green
                { .rot =  8, .nbits = 5-1 }, // red
            } // rrrrrggggggbbbbb
        }     // ........Pppppppp
    },
    // DVI_PIXEL_FORMAT_XRGB1555
    {
        .pixels_per_word = 2, .bits_per_pixel = 16,
        .expand_tmds = {
            .lane = {
                { .rot = 29, .nbits = 5-1 }, // blue
                { .rot =  2, .nbits = 5-1 }, // green
                { .rot =  7, .nbits = 5-1 }, // red
            } // xrrrrrgggggbbbbb
        }     // ........Pppppppp
    },
    // DVI_PIXEL_FORMAT_XRGB4444
    {
        .pixels_per_word = 2, .bits_per_pixel = 16,
        .expand_tmds = {
            .lane = {
                { .rot = 28, .nbits = 4-1 }, // blue
                { .rot =  0, .nbits = 4-1 }, // green
                { .rot =  4, .nbits = 4-1 }, // red
            } // xxxxrrrrggggbbbb
        }     // ........Pppppppp
    },
    // DVI_PIXEL_FORMAT_RGB332
    {
        .pixels_per_word = 4, .bits_per_pixel = 8,
        .expand_tmds = {
            .lane = {
                { .rot = 26, .nbits = 2-1 }, // blue
                { .rot = 29, .nbits = 3-1 }, // green
                { .rot =  0, .nbits = 3-1 }, // red
            } // xxxxxxxxRrrGggBb
        }     // ........Pppppppp
    },
    // DVI_PIXEL_FORMAT_XRGB2222
    {
        .pixels_per_word = 4, .bits_per_pixel = 8,
        .expand_tmds = {
            .lane = {
                { .rot = 26, .nbits = 2-1 }, // blue
                { .rot = 28, .nbits = 2-1 }, // green
                { .rot = 30, .nbits = 2-1 }, // red
            } // xxxxxxxxxxRrGgBb
        }     // ........Pppppppp
    },
    // DVI_PIXEL_FORMAT_GRAY8
    {
        .pixels_per_word = 4, .bits_per_pixel = 8,
        .expand_tmds = {
            .lane = {
                { .rot = 0, .nbits = 8-1 }, // blue
                { .rot = 0, .nbits = 8-1 }, // green
                { .rot = 0, .nbits = 8-1 }, // red
            }
        }     // ........Pppppppp
    },
};

// ---------------------------

enum {
    DMA_CTRL_BLANK          = 0,
    DMA_CTRL_ACTIVE         = 1,

    DMA_CTRL_RESET          = (1 << 1),

    DMA_CTRL_BLANK_RESET    = DMA_CTRL_BLANK  | DMA_CTRL_RESET,
    DMA_CTRL_ACTIVE_RESET   = DMA_CTRL_ACTIVE | DMA_CTRL_RESET,

    DMA_CTRL_LAST,
};

// DMA channels used for the output and repeat logic
struct dvi_dma_info_t {
    uint    channel;
    dma_channel_hw_t * hw;  // pointer to channel register block
    uint    dst_cmds;
    uint    dst_pixels;
    uint    src_pixels;     // PIO->DMA case only
    
    uint32_t ctrl[DMA_CTRL_LAST];   // control words
};

struct dvi_dma_info_short_t {
    uint    channel;
    dma_channel_hw_t * hw;  // pointer to channel register block
};

// ---------------------------
// internal context - work in progress
struct dvi_hstx_context_t {
    // DMA channel info
    struct {
        // SRAM/PIO->HSTX channel
        struct dvi_dma_info_t       hstx;
        // command list walking
        struct dvi_dma_info_short_t cmdlist;
        // command list reset
        struct dvi_dma_info_short_t cmdlist_reset;
        // SRAM->PIO channel
        struct dvi_dma_info_short_t pio;
    } dma;
    
    struct {
        // current position in command list
        uint32_t cmdlist_offset;

        // line repeat count and reset value
        uint32_t line_repeat;
        uint32_t line_repeat_preset;

        // A ping and a pong are cued up initially, so the first time we enter this
        // handler it is to cue up the second ping after the first ping has completed.
        // This is the third scanline overall (-> =2 because zero-based).
        uint32_t scanline;

        // start address, incremented by pitch every active scanline
        uint32_t start_address;

        // total frames displayed counter
        uint32_t frames;

        // state machine phase
        int state;

        // next states for state machine
        int backporch_next_state;
        int active_next_state;

        // scanline check points
        int sync_end;           // front_porch + sync
        int back_porch_end;     // sync_end + back_porch
        int border_top_end;     // back_porch_end + border_top
        int active_end;         // border_top_end + active
        int border_bottom_end;  // active_end + border_bottom
    } irq_sm;

    struct {
        // frame buffer stuff
        // latched values are copied to actual every vblank
        volatile void       *fb,   *fb_latch;
        volatile uint32_t    pitch, pitch_latch;
        volatile uint32_t    pan_instr, pan_instr_latch;
    };

    struct {
        // transfer size for active line
        uint32_t hstx;
        uint32_t pio;
    } active_xfer_count;

    struct {
        PIO hw;             // pointer to registers
        pio_sm_hw_t* sm;    // pointer to state machine register
        int sm_idx;         // state machine index
        int instr_offset;   // offset for a program
        int rep_count;      // replication count
    } pio;

    // IRQ line used for DMA chaining
    int irq;

    // transfer mode
    int xfer_mode;

    // copy of user dvi_timings_t
    struct dvi_timings_t timings;

    // HSTX input format
    struct dvi_hstx_input_params_t  hstx_input;

    // bus error info storage
    volatile struct dvi_bus_error_t bus_error;
};

// internal context
static struct dvi_hstx_context_t v_ctx;

#if 0
// handle DMA bus error from IRQ handler
__attribute__ ((noinline)) void dvi_dma_handle_bus_error() {
    // get offending channel number
    int chnum = v_ctx.dma.chain[0].hw->ctrl_trig & DMA_CH0_CTRL_TRIG_AHB_ERROR_BITS ? 0 : 1;
    
    // save state
    v_ctx.bus_error.state               = v_ctx.irq_sm.state;
    v_ctx.bus_error.scanline            = v_ctx.irq_sm.scanline;

    // save registers
    v_ctx.bus_error.num                 = v_ctx.dma.chain[chnum].channel;
    v_ctx.bus_error.regs.read_addr      = v_ctx.dma.chain[chnum].hw->read_addr;
    v_ctx.bus_error.regs.write_addr     = v_ctx.dma.chain[chnum].hw->write_addr;
    v_ctx.bus_error.regs.transfer_count = v_ctx.dma.chain[chnum].hw->transfer_count;
    v_ctx.bus_error.regs.ctrl           = v_ctx.dma.chain[chnum].hw->ctrl_trig;

    // stop video output
    dvi_stop_dma();
}
#endif

static void __not_in_flash_func(dvi_state_advance)(struct dvi_hstx_context_t* ctx, struct dvi_dma_command_list_t *cmdlist, uint32_t ctrl_ofs) {
    switch (v_ctx.irq_sm.state) {
        case DVI_STATE_IDLE:
            break;
        case DVI_STATE_FRONT_PORCH:
            cmdlist->read_addr      = (uintptr_t)&hstx_script_blank;
            cmdlist->write_addr     = (uintptr_t)&hstx_fifo_hw->fifo;
            cmdlist->trans_count    = sizeof(hstx_script_blank)/sizeof(uint32_t);
            cmdlist->ctrl_trig      = ctx->dma.hstx.ctrl[DMA_CTRL_BLANK + ctrl_ofs];
            v_ctx.irq_sm.scanline++;
            if (v_ctx.irq_sm.scanline == v_ctx.timings.v.front_porch)
                v_ctx.irq_sm.state = DVI_STATE_SYNC;
            break;
        case DVI_STATE_SYNC:
            cmdlist->read_addr      = (uintptr_t)&hstx_script_sync;
            cmdlist->write_addr     = (uintptr_t)&hstx_fifo_hw->fifo;
            cmdlist->trans_count    = sizeof(hstx_script_sync)/sizeof(uint32_t);
            cmdlist->ctrl_trig      = ctx->dma.hstx.ctrl[DMA_CTRL_BLANK + ctrl_ofs];
            v_ctx.irq_sm.scanline++;
            if (v_ctx.irq_sm.scanline == v_ctx.irq_sm.sync_end)
                v_ctx.irq_sm.state = DVI_STATE_BACK_PORCH;
            break;
        case DVI_STATE_BACK_PORCH:
            cmdlist->read_addr      = (uintptr_t)&hstx_script_blank;
            cmdlist->write_addr     = (uintptr_t)&hstx_fifo_hw->fifo;
            cmdlist->trans_count    = sizeof(hstx_script_blank)/sizeof(uint32_t);
            cmdlist->ctrl_trig      = ctx->dma.hstx.ctrl[DMA_CTRL_BLANK + ctrl_ofs];
            v_ctx.irq_sm.scanline++;
            if (v_ctx.irq_sm.scanline == v_ctx.irq_sm.back_porch_end) {
                v_ctx.irq_sm.state = v_ctx.irq_sm.backporch_next_state;
                v_ctx.irq_sm.start_address = 0;
                v_ctx.fb    = v_ctx.fb_latch;
                v_ctx.pitch = v_ctx.pitch_latch;
                v_ctx.pan_instr = v_ctx.pan_instr_latch;
            }
            break;
        case DVI_STATE_TOP_BORDER:
            cmdlist->read_addr      = (uintptr_t)&hstx_script_border;
            cmdlist->write_addr     = (uintptr_t)&hstx_fifo_hw->fifo;
            cmdlist->trans_count    = sizeof(hstx_script_border)/sizeof(uint32_t);
            cmdlist->ctrl_trig      = ctx->dma.hstx.ctrl[DMA_CTRL_BLANK + ctrl_ofs];
            v_ctx.irq_sm.scanline++;
            if (v_ctx.irq_sm.scanline == v_ctx.irq_sm.border_top_end)
                v_ctx.irq_sm.state = DVI_STATE_ACTIVE_BLANK;
            break;
        case DVI_STATE_ACTIVE_BLANK:
            // active line, horizontal blank
            cmdlist->read_addr      = (uintptr_t)&hstx_script_active;
            cmdlist->write_addr     = (uintptr_t)&hstx_fifo_hw->fifo;
            cmdlist->trans_count    = sizeof(hstx_script_active)/sizeof(uint32_t);
            cmdlist->ctrl_trig      = ctx->dma.hstx.ctrl[DMA_CTRL_BLANK + ctrl_ofs];
            v_ctx.irq_sm.state      = DVI_STATE_ACTIVE_PIXELS;
            break;
        case DVI_STATE_ACTIVE_PIXELS:
            // active line, active pixels
            // TODO: handle SRAM->HSTX, SRAM->PIO->HSTX and SRAM->PIO->CLUT->HSTX cases
            uintptr_t line_ptr = (uintptr_t)((volatile uint8_t*)v_ctx.fb + v_ctx.irq_sm.start_address);
            switch (v_ctx.xfer_mode) {
                case DVI_XFER_MODE_SRAM_HSTX:
                    cmdlist->read_addr      = line_ptr;
                    cmdlist->write_addr     = (uintptr_t)&hstx_fifo_hw->fifo;
                    cmdlist->trans_count    = v_ctx.active_xfer_count.hstx;
                    cmdlist->ctrl_trig      = ctx->dma.hstx.ctrl[DMA_CTRL_ACTIVE + ctrl_ofs];
                    break;
                case DVI_XFER_MODE_SRAM_PIO_HSTX:
#if 0               // TODO: must use new logic, yet to implement it
                    // setup PIO->HSTX DMA channel
                    cmdlist->read_addr      = ctx->dma.hstx.src_pixels;
                    cmdlist->trans_count    = v_ctx.active_xfer_count.hstx;
                    cmdlist->ctrl_trig      = ctx->dma.hstx.ctrl_pixels;

                    // then start PIO
                    pio_sm_set_enabled(v_ctx.pio.hw, v_ctx.pio.sm_idx, false);
                    pio_sm_restart(v_ctx.pio.hw, v_ctx.pio.sm_idx);
                    pio_sm_clear_fifos(v_ctx.pio.hw, v_ctx.pio.sm_idx);
                    pio_sm_exec(v_ctx.pio.hw, v_ctx.pio.sm_idx, v_ctx.pan_instr);
                    pio_sm_set_enabled(v_ctx.pio.hw, v_ctx.pio.sm_idx, true);

                    // and start SRAM->PIO DMA
                    v_ctx.dma.pio.hw->al1_read_addr = line_ptr;
                    v_ctx.dma.pio.hw->al1_transfer_count_trig = v_ctx.active_xfer_count.pio;
                    break;
#endif
                default:
                    break;
            }
            
            if (!--v_ctx.irq_sm.line_repeat) {
                v_ctx.irq_sm.line_repeat = v_ctx.irq_sm.line_repeat_preset;
                v_ctx.irq_sm.start_address += v_ctx.pitch;
            }
            v_ctx.irq_sm.scanline++;
            if (v_ctx.irq_sm.scanline == v_ctx.irq_sm.active_end)
                v_ctx.irq_sm.state = v_ctx.irq_sm.active_next_state;
            else
                v_ctx.irq_sm.state = DVI_STATE_ACTIVE_BLANK;
            break;
        case DVI_STATE_BOTTOM_BORDER:
            cmdlist->read_addr      = (uintptr_t)&hstx_script_border;
            cmdlist->write_addr     = (uintptr_t)&hstx_fifo_hw->fifo;
            cmdlist->trans_count    = sizeof(hstx_script_border)/sizeof(uint32_t);
            cmdlist->ctrl_trig      = ctx->dma.hstx.ctrl[DMA_CTRL_BLANK + ctrl_ofs];
            v_ctx.irq_sm.scanline++;
            if (v_ctx.irq_sm.scanline == v_ctx.irq_sm.border_top_end) 
                v_ctx.irq_sm.state = DVI_STATE_FRONT_PORCH;
            break;
    }

    // wrap scanline counter
    if (v_ctx.irq_sm.scanline >= v_ctx.timings.v.total) {
        v_ctx.irq_sm.scanline = 0;
        v_ctx.irq_sm.frames++;
    }
}

static void __scratch_y("")(dvi_fill_command_list)() {
    struct dvi_hstx_context_t* ctx = &v_ctx;
    struct dvi_dma_command_list_t *cmdlist = dvi_command_list + v_ctx.irq_sm.cmdlist_offset;
    for (int i = 0; i < COMMAND_LIST_SIZE; i++) {
        dvi_state_advance(ctx, cmdlist++, (i == COMMAND_LIST_SIZE-1) ? DMA_CTRL_RESET : 0);
    }
    v_ctx.irq_sm.cmdlist_offset ^= COMMAND_LIST_SIZE;
}

// DMA IRQ handler
void __scratch_y("")(dvi_dma_irq_handler)() {
#if 0
    // first test if bus error is encountered
    if ((v_ctx.dma.chain[0].hw->ctrl_trig | v_ctx.dma.chain[1].hw->ctrl_trig) & DMA_CH0_CTRL_TRIG_AHB_ERROR_BITS) {
        return dvi_dma_handle_bus_error();
    }
#endif
    
    // DMA runs on its own, so we don't need to care much. just acknowledge an IRQ and
    // fill the command list with new values

    // TODO: handle HSTX channel IRQ for restarting SRAM->PIO channel (pixel doubling etc)

    dvi_fill_command_list();
    dma_hw->intr = 1u << v_ctx.dma.cmdlist_reset.channel; // faster + no need to track which IRQ line is used
}

// init DVI context
void dvi_ctx_init() {
    memset(&v_ctx, 0, sizeof(struct dvi_hstx_context_t));

    // setup common fields
    v_ctx.irq_sm.line_repeat = 
    v_ctx.irq_sm.line_repeat_preset = 1;
    v_ctx.pan_instr_latch = pio_encode_nop();

    // clear bus error handler
    v_ctx.bus_error.num = -1;
}

// set timings
// returns true if there is an error, else 0 and dvi_timings_t updated
int dvi_set_timings(struct dvi_timings_t *t, int flags) {
    if (v_ctx.irq_sm.state != DVI_STATE_IDLE) return 1;        // stop DVI output before tweaking!
    
    // validate fields
    if (t->h.front_porch <= 0 || t->h.sync <= 0 || t->h.back_porch <= 0 || t->h.active <= 0) return 1;
    if (t->v.front_porch <= 3 || t->v.sync <= 0 || t->v.back_porch <= 0 || t->v.active <= 0) return 1;

    // calculate total lines/pixels
    t->h.total = t->h.front_porch + t->h.sync + t->h.back_porch + t->h.active;
    t->v.total = t->v.front_porch + t->v.sync + t->v.back_porch + t->v.border_top + t->v.active + t->v.border_bottom;

    // calculate pixel clock
    if (t->pixelclock == 0) {
        t->pixelclock = clock_get_hz(clk_hstx)/5;
    }

    // calculate refresh rate
    int32_t current_refresh = (((t->pixelclock) / (t->v.total)) * 1000) / t->h.total;
    if (t->v.refresh == 0 || t->v.refresh >= current_refresh) {
        t->v.refresh = current_refresh;
    } else if (t->v.refresh < current_refresh) {
        if (flags & DVI_SET_TIMINGS_REFRESH_HBLANK) {
            // adjust h front porch for current pixelclock
            int32_t new_htotal = (((int64_t)t->h.total) * current_refresh + 1000 - 1) / t->v.refresh;
            t->h.front_porch += new_htotal - t->h.total;
            // recalculate refresh rate
            t->v.refresh = (((t->pixelclock) / (new_htotal)) * 1000) / t->v.total;
            t->h.total = new_htotal;
        } else {
            // adjust v front porch for current pixelclock
            int32_t new_vtotal = (((int64_t)t->v.total) * current_refresh + 1000 - 1) / t->v.refresh;
            t->v.front_porch += new_vtotal - t->v.total;
            // recalculate refresh rate
            t->v.refresh = (((t->pixelclock) / (t->h.total)) * 1000) / new_vtotal;
            t->v.total = new_vtotal;
        }
        
    } else return 1;

    // calculate scanline check points
    v_ctx.irq_sm.sync_end           = t->v.front_porch              + t->v.sync;
    v_ctx.irq_sm.back_porch_end     = v_ctx.irq_sm.sync_end         + t->v.back_porch;
    v_ctx.irq_sm.border_top_end     = v_ctx.irq_sm.back_porch_end   + t->v.border_top;
    v_ctx.irq_sm.active_end         = v_ctx.irq_sm.border_top_end   + t->v.active;
    v_ctx.irq_sm.border_bottom_end  = v_ctx.irq_sm.active_end       + t->v.border_bottom;

    // and states
    v_ctx.irq_sm.backporch_next_state = t->v.border_top    > 0 ? DVI_STATE_TOP_BORDER    : DVI_STATE_ACTIVE_BLANK;
    v_ctx.irq_sm.active_next_state    = t->v.border_bottom > 0 ? DVI_STATE_BOTTOM_BORDER : DVI_STATE_FRONT_PORCH;

    // update command lists
    hstx_script_blank.front_porch.token.length       =
    hstx_script_sync.front_porch.token.length        = 
    hstx_script_border.front_porch.token.length      = 
    hstx_script_border_hdmi.front_porch.token.length = 
    hstx_script_active.front_porch.token.length      = 
    hstx_script_active_hdmi.front_porch.token.length = t->h.front_porch;

    hstx_script_blank.sync.token.length              =
    hstx_script_sync.sync.token.length               = 
    hstx_script_border.sync.token.length             = 
    hstx_script_border_hdmi.sync.token.length        = 
    hstx_script_active.sync.token.length             = 
    hstx_script_active_hdmi.sync.token.length        = t->h.sync;
 
    hstx_script_border.back_porch.token.length       = 
    hstx_script_border_hdmi.back_porch.token.length  = 
    hstx_script_active.back_porch.token.length       = 
    hstx_script_active_hdmi.back_porch.token.length  = t->h.back_porch;

    hstx_script_blank.back_porch.token.length   =
    hstx_script_sync.back_porch.token.length    = t->h.back_porch + t->h.active;

    hstx_script_border.border.token.length      = 
    hstx_script_border_hdmi.border.token.length = 
    hstx_script_active.active.length            = 
    hstx_script_active_hdmi.active.length       = t->h.active;

    // set polarity
    uint32_t syncpol = (t->flags & DVI_TIMINGS_SYNC_POLARITY_MASK);
    hstx_script_blank.front_porch.data          = 
    hstx_script_border.front_porch.data         = 
    hstx_script_border_hdmi.front_porch.data    = 
    hstx_script_active.front_porch.data         = 
    hstx_script_active_hdmi.front_porch.data    = 
    hstx_script_blank.back_porch.data           = 
    hstx_script_border.back_porch.data          = 
    hstx_script_border_hdmi.back_porch.data     = 
    hstx_script_active.back_porch.data          = 
    hstx_script_active_hdmi.back_porch.data     = dvi_sync_table[syncpol];

    hstx_script_blank.sync.data                 = 
    hstx_script_border.sync.data                = 
    hstx_script_border_hdmi.sync.data           = 
    hstx_script_active.sync.data                = 
    hstx_script_active_hdmi.sync.data           = dvi_sync_table[syncpol ^ DVI_TIMINGS_H_NEG];

    hstx_script_sync.front_porch.data           = 
    hstx_script_sync.back_porch.data            = dvi_sync_table[syncpol ^ DVI_TIMINGS_V_NEG];
    hstx_script_sync.sync.data                  = dvi_sync_table[syncpol ^ DVI_TIMINGS_V_NEG ^ DVI_TIMINGS_H_NEG];

    hstx_script_border_hdmi.preamble.data       = 
    hstx_script_active_hdmi.preamble.data       = HDMI_VIDEO_PREAMBLE(dvi_ctrl_table[syncpol]);

    // copy struct
    memcpy(&v_ctx.timings, t, sizeof(struct dvi_timings_t));

    return 0;
}

void dvi_set_line_repeat(int repeat) {
    v_ctx.irq_sm.line_repeat = v_ctx.irq_sm.line_repeat_preset = repeat;
}

// set border color in _current_HSTX_color_format_ - can be done at any moment (in theory :)
void dvi_set_border_color(uint32_t color) {
    hstx_script_border.border.data = color;
    hstx_script_border_hdmi.border.data = color;
}

// set frame buffer pointer
void __not_in_flash_func(dvi_set_framebuffer)(void *ptr, int immediate) {
    v_ctx.fb_latch          = ptr;
    if (immediate) v_ctx.fb = ptr;
}

// set framebuffer pitch
void __not_in_flash_func(dvi_set_pitch)(uint32_t pitch, int immediate) {
    v_ctx.pitch_latch           = pitch;
    if (immediate) v_ctx.pitch  = pitch;
}

// set current offset in framebuffer (WILL cause tearing, use only with careful beam racing!)
void __not_in_flash_func(dvi_set_offset)(uint32_t offset) {
    v_ctx.irq_sm.start_address = offset;
}

void __not_in_flash_func(dvi_set_pixel_panning)(uint32_t offset, int immediate) {
    uint32_t bit_offset = (offset*v_ctx.hstx_input.bits_per_pixel) & 31;
    if (bit_offset == 0) {
        v_ctx.pan_instr_latch = pio_encode_nop();
    } else {
        v_ctx.pan_instr_latch = pio_encode_out(pio_null, bit_offset);
    }
    if (immediate) v_ctx.pan_instr = v_ctx.pan_instr_latch;
}

// get current state
int __not_in_flash_func(dvi_get_current_state)() {
    return *(volatile int*)&v_ctx.irq_sm.state;
}

// get frame count
uint32_t __not_in_flash_func(dvi_get_frame_count)() {
    return *(volatile int*)&v_ctx.irq_sm.frames;
}

uint32_t __not_in_flash_func(dvi_get_current_scanline)() {
    return *(volatile int*)&v_ctx.irq_sm.scanline;
}

// check if bus error occured
int dvi_is_bus_error() {
    return (v_ctx.bus_error.num != -1) ? true : false;
}

// get bus error struct
volatile struct dvi_bus_error_t* dvi_get_bus_error_info() {
    return &v_ctx.bus_error;
}

// block until blank
void __not_in_flash_func(dvi_wait_for_vblank)() {
    while (dvi_get_current_state() != DVI_STATE_IDLE && dvi_get_current_state() == DVI_STATE_FRONT_PORCH) tight_loop_contents();
    while (dvi_get_current_state() != DVI_STATE_IDLE && dvi_get_current_state() != DVI_STATE_FRONT_PORCH) tight_loop_contents();
}

// block until hblank
void __not_in_flash_func(dvi_wait_for_hblank()) {
    while (dvi_get_current_state() != DVI_STATE_IDLE && dvi_get_current_state() == DVI_STATE_ACTIVE_BLANK) tight_loop_contents();
    while (dvi_get_current_state() != DVI_STATE_IDLE && dvi_get_current_state() != DVI_STATE_ACTIVE_BLANK) tight_loop_contents();
}

// kludge but i don't care
static uint32_t dvi_pixels_per_word_to_dma_size(uint32_t pixels_per_word) {
    switch(pixels_per_word) {
        case 1: return DMA_SIZE_8;
        case 2: return DMA_SIZE_16;
        case 4: return DMA_SIZE_32;
        default: return -1;
    }
}

int dvi_get_resources_required(int src_format, int pal_format, int flags, struct dvi_resources_t *res) {
    if (res == NULL) return 1;
    if (src_format == DVI_PIXEL_FORMAT_NULL || src_format > DVI_PIXEL_FORMAT_DIRECT_END) return 1; // palettized formats are not supported yet

    switch (flags & DVI_PIXEL_REP_MASK) {
        case DVI_PIXEL_REP_1:
            if ((flags & DVI_PIXEL_SMOOTH_PANNING) && (src_format != DVI_PIXEL_FORMAT_XRGB8888)) {
                res->dma_channels_num   = 4; // SRAM --w--> PIO --w--> HSTX
                res->pio_required       = 1;
                res->xfer_mode          = DVI_XFER_MODE_SRAM_PIO_HSTX; 
                res->hstx_dma_size      = DMA_SIZE_32;
            } else {
                res->dma_channels_num   = 3; // SRAM --w--> HSTX
                res->pio_required       = 0;
                res->xfer_mode          = DVI_XFER_MODE_SRAM_HSTX; 
                res->hstx_dma_size      = DMA_SIZE_32;
            }
            break;
        case DVI_PIXEL_REP_2:
            if (dvi_hstx_pixel_formats[src_format].pixels_per_word >= 2) {
                res->dma_channels_num   = 3; // SRAM -h/w-> HSTX
                res->pio_required       = 0;
                res->xfer_mode          = DVI_XFER_MODE_SRAM_HSTX; 
                res->hstx_dma_size      = dvi_pixels_per_word_to_dma_size(dvi_hstx_pixel_formats[src_format].pixels_per_word);
            } else {
                res->dma_channels_num   = 4; // SRAM --w--> PIO --w--> HSTX
                res->pio_required       = 1;
                res->xfer_mode          = DVI_XFER_MODE_SRAM_PIO_HSTX; 
                res->hstx_dma_size      = DMA_SIZE_32;
            }
            break;
        case DVI_PIXEL_REP_4:
            res->dma_channels_num   = 3; // SRAM -bhw-> HSTX
            res->pio_required       = 0;
            res->xfer_mode          = DVI_XFER_MODE_SRAM_HSTX; 
            res->hstx_dma_size      = dvi_pixels_per_word_to_dma_size(dvi_hstx_pixel_formats[src_format].pixels_per_word);
            break;
        default:
            return 1;
    }
    res->pio = NULL;
    res->sm = -1;       // unspecified
    return 0;
}

int dvi_configure_xfer_mode(int mode) {
    if (mode >= DVI_XFER_MODE_END) return 1;
    v_ctx.xfer_mode = mode;
    return 0;
}

struct dvi_pio_config_t {
    const pio_program_t* prog;
    uint8_t        wrap, wrap_target;
    uint8_t        pix_in_width, pix_out_width;
};

enum {
    DVI_PIO_REP1_8BPP,
    DVI_PIO_REP1_16BPP,
    DVI_PIO_REP2_8BPP,
    DVI_PIO_REP2_16BPP,
};

static const struct dvi_pio_config_t dvi_pio_config[] = {
    // DVI_PIO_REP1_8BPP
    {
        .prog = &dvi_rep1_8bpp_program,
        .wrap = dvi_rep1_8bpp_wrap, .wrap_target = dvi_rep1_8bpp_wrap_target,
        .pix_in_width = 32, .pix_out_width = 32
    },
    // DVI_PIO_REP1_16BPP
    {
        .prog = &dvi_rep1_16bpp_program,
        .wrap = dvi_rep1_16bpp_wrap, .wrap_target = dvi_rep1_16bpp_wrap_target,
        .pix_in_width = 32, .pix_out_width = 32
    },
    // DVI_PIO_REP2_8BPP
    {
        .prog = &dvi_rep2_8bpp_program,
        .wrap = dvi_rep2_8bpp_wrap, .wrap_target = dvi_rep2_8bpp_wrap_target,
        .pix_in_width = 32, .pix_out_width = 32
    },
    // DVI_PIO_REP2_16BPP
    {
        .prog = &dvi_rep2_16bpp_program,
        .wrap = dvi_rep2_16bpp_wrap, .wrap_target = dvi_rep2_16bpp_wrap_target,
        .pix_in_width = 32, .pix_out_width = 32
    }
};

int dvi_configure_pio(int src_format, int pal_format, int flags, struct dvi_resources_t *res) {
    if (res == NULL || res->pio == NULL) return 1;
    if (res->pio_required == 0) return 0;       // no need to configure PIO so skip it

    // select PIO program for execution
    pio_sm_config config = pio_get_default_sm_config();
    const struct dvi_pio_config_t *dvi_pio_prog = dvi_pio_config + DVI_PIO_REP1_8BPP;

    switch (res->xfer_mode) {
        case DVI_XFER_MODE_SRAM_PIO_HSTX:
            switch (flags & DVI_PIXEL_REP_MASK) {
                case DVI_PIXEL_REP_1:
                    switch(dvi_hstx_pixel_formats[src_format].bits_per_pixel) {
                        case 8:  dvi_pio_prog = dvi_pio_config + DVI_PIO_REP1_8BPP; break;
                        case 16: dvi_pio_prog = dvi_pio_config + DVI_PIO_REP1_16BPP; break;
                        default: break;
                    }
                    v_ctx.pio.rep_count = 1;
                    break;
                case DVI_PIXEL_REP_2:
                    switch(dvi_hstx_pixel_formats[src_format].bits_per_pixel) {
                        case 8:  dvi_pio_prog = dvi_pio_config + DVI_PIO_REP2_8BPP; break;
                        case 16: dvi_pio_prog = dvi_pio_config + DVI_PIO_REP2_16BPP; break;
                        default: break;
                    }
                    v_ctx.pio.rep_count = 2;
                    break;
                default:
                    break;
            }
            break;
        default:
            return 1;   // not supported!
    }

    // first, find unused state machine if user hadn't supplied one
    if (res->sm == -1) {
        for (res->sm = 0; res->sm < NUM_PIO_STATE_MACHINES; res->sm++) {
            if (pio_sm_is_claimed(res->pio, res->sm) == false) break;
        }
        if (res->sm == NUM_PIO_STATE_MACHINES) return 1;    // all SMs are claimed - nothing to add!
    }

    // claim state machine
    pio_sm_claim(res->pio, res->sm);

    // target SM is found, now check if we can add a program
    if (!pio_can_add_program(res->pio, dvi_pio_prog->prog)) return 1;

    // then add a program
    int instr_offset = pio_add_program(res->pio, dvi_pio_prog->prog);
    if (instr_offset < 0) goto err_unclaim_sm;

    // and configure state machine
    sm_config_set_wrap(&config, instr_offset + dvi_pio_prog->wrap_target, instr_offset + dvi_pio_prog->wrap);
    sm_config_set_in_shift(&config, 1, 1, dvi_pio_prog->pix_out_width);
    sm_config_set_out_shift(&config, 1, 1, dvi_pio_prog->pix_in_width);     // NB!!!!
    sm_config_set_out_pins(&config, 0, 0);
    sm_config_set_out_pin_count(&config, 0);
    if (pio_sm_init(res->pio, res->sm, instr_offset, &config) < 0) goto err_remove_pgm;

    // phew - almost done :) clear FIFOs and save configuration
    pio_sm_clear_fifos(res->pio, res->sm);
    v_ctx.pio.hw = res->pio;
    v_ctx.pio.sm_idx = res->sm;
    v_ctx.pio.sm = res->pio->sm + res->sm;
    v_ctx.pio.instr_offset = instr_offset;

    return 0;

    // error handling
err_remove_pgm:
    pio_remove_program(res->pio, dvi_pio_prog->prog, instr_offset);
err_unclaim_sm:
    pio_sm_unclaim(res->pio, res->sm);
    return 1;
}

// configure HSTX input
int dvi_configure_hstx_input(int format, int flags, int xfer_mode, const struct dvi_hstx_input_params_t *ptr) {
    if (format >= DVI_PIXEL_FORMAT_DIRECT_END) return 1;
    if (format != DVI_PIXEL_FORMAT_NULL) ptr = dvi_hstx_pixel_formats + format;

    // save format
    memcpy(&v_ctx.hstx_input, ptr, sizeof(struct dvi_hstx_input_params_t));

    // configure HSTX for given pixel format
    hstx_ctrl_hw->expand_tmds = ptr->expand_tmds.raw;
    uint32_t expand_shift = 
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

    int hstx_rep_count = (xfer_mode == DVI_XFER_MODE_SRAM_PIO_HSTX) ? 0 : 1 << (flags & DVI_PIXEL_REP_MASK); // only power of two replication allowed

    if (hstx_rep_count > 1) {
        expand_shift |=
            (hstx_rep_count << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB) |
            (0 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB);
        v_ctx.hstx_input.rep_count       = hstx_rep_count;
        v_ctx.hstx_input.pixels_per_word = 1;
    } else {
        expand_shift |=
            (ptr->pixels_per_word << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB) |
            (ptr->bits_per_pixel  << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB);
        v_ctx.hstx_input.rep_count = 1;
    }
    hstx_ctrl_hw->expand_shift = expand_shift;

    return 0;
}

// configure HSTX output
void dvi_configure_hstx_output(union dvi_hstx_pin_layout_t layout) {
#ifndef HSTX_SERIAL_DEBUG
    // Serial output config: clock period of 5 cycles, pop from command
    // expander every 5 cycles, shift the output shiftreg by 2 every cycle.
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    // assign pins
    hstx_ctrl_hw->bit[layout.clock_n] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.clock_p] = HSTX_CTRL_BIT0_CLK_BITS;

    hstx_ctrl_hw->bit[layout.lane0_n] = ((0*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((0*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB) | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.lane0_p] = ((0*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((0*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[layout.lane1_n] = ((1*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((1*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB) | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.lane1_p] = ((1*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((1*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[layout.lane2_n] = ((2*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((2*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB) | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.lane2_p] = ((2*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((2*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB);
#else
    // Serial output config: clock period of 1 cycle, pop from command
    // expander every 5 cycles, shift the output shiftreg by 2 every cycle.
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        1u << HSTX_CTRL_CSR_CLKDIV_LSB |
        10u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        1u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    // assign pins
    hstx_ctrl_hw->bit[layout.clock_n] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.clock_p] = HSTX_CTRL_BIT0_CLK_BITS;

    hstx_ctrl_hw->bit[layout.lane0_n] = ((0*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((0*10 + 0) << HSTX_CTRL_BIT0_SEL_N_LSB) | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.lane0_p] = ((0*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((0*10 + 0) << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[layout.lane1_n] = ((1*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((1*10 + 0) << HSTX_CTRL_BIT0_SEL_N_LSB) | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.lane1_p] = ((1*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((1*10 + 0) << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[layout.lane2_n] = ((2*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((2*10 + 0) << HSTX_CTRL_BIT0_SEL_N_LSB) | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.lane2_p] = ((2*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((2*10 + 0) << HSTX_CTRL_BIT0_SEL_N_LSB);
#endif

    // set pin function to HSTX
    for (int i = 12; i <= 19; ++i) {
        gpio_set_function(i, 0); // HSTX
    }
}

// init DMA for the display output
int dvi_init_dma(const struct dvi_resources_t* res) {
    assert(res->dma_irq_line >= DMA_IRQ_0 && res->dma_irq_line <= DMA_IRQ_3);
    // IRQ line must be exclusive!
    assert(irq_get_exclusive_handler(res->dma_irq_line) == NULL);
    v_ctx.irq = res->dma_irq_line;

    // allocate DMA channel numbers
    v_ctx.dma.cmdlist.channel       = res->dma_channels[0];
    v_ctx.dma.cmdlist.hw            = dma_channel_hw_addr(v_ctx.dma.cmdlist.channel);
    v_ctx.dma.cmdlist_reset.channel = res->dma_channels[1];
    v_ctx.dma.cmdlist_reset.hw      = dma_channel_hw_addr(v_ctx.dma.cmdlist_reset.channel);
    v_ctx.dma.hstx.channel          = res->dma_channels[2];
    v_ctx.dma.hstx.hw               = dma_channel_hw_addr(v_ctx.dma.hstx.channel);

    // setup command list channel - the SRAM->HSTX channel will be started automatically
    dma_channel_config dma_cmdlist_cfg = dma_channel_get_default_config(v_ctx.dma.cmdlist.channel);
    channel_config_set_read_increment (&dma_cmdlist_cfg, true);
    channel_config_set_write_increment(&dma_cmdlist_cfg, true);
    channel_config_set_transfer_data_size(&dma_cmdlist_cfg, DMA_SIZE_32);
    channel_config_set_ring(&dma_cmdlist_cfg, true, 4);      // wrap writes on DMA control block
    dma_channel_configure(
        v_ctx.dma.cmdlist.channel,
        &dma_cmdlist_cfg,
        v_ctx.dma.hstx.hw,
        &dvi_command_list[0],
        4,
        false
    );

    // setup command list reset channel
    dma_channel_config dma_clreset_cfg = dma_channel_get_default_config(v_ctx.dma.cmdlist_reset.channel);
    channel_config_set_read_increment (&dma_clreset_cfg, true);
    channel_config_set_write_increment(&dma_clreset_cfg, false);
    channel_config_set_transfer_data_size(&dma_clreset_cfg, DMA_SIZE_32);
    channel_config_set_ring(&dma_clreset_cfg, false, 3);      // wrap reads on 2 read addresses
    dma_channel_configure(
        v_ctx.dma.cmdlist_reset.channel,
        &dma_clreset_cfg,
        &v_ctx.dma.cmdlist.hw->al3_read_addr_trig,
        &dma_command_list_read_addr[0],
        1,
        false
    );

    // setup SRAM->HSTX channel, calculate command words
    dma_channel_config dma_config_hstx = dma_channel_get_default_config(v_ctx.dma.hstx.channel);
    channel_config_set_dreq(&dma_config_hstx, DREQ_HSTX);
    channel_config_set_read_increment (&dma_config_hstx, true);
    channel_config_set_write_increment(&dma_config_hstx, false);
    channel_config_set_transfer_data_size(&dma_config_hstx, DMA_SIZE_32);
    channel_config_set_chain_to(&dma_config_hstx, v_ctx.dma.cmdlist.channel);
    v_ctx.dma.hstx.ctrl[DMA_CTRL_BLANK] = 
        dma_config_hstx.ctrl;
    v_ctx.dma.hstx.ctrl[DMA_CTRL_BLANK | DMA_CTRL_RESET] =
        dma_config_hstx.ctrl;
    channel_config_set_transfer_data_size(&dma_config_hstx, res->hstx_dma_size);
    v_ctx.dma.hstx.ctrl[DMA_CTRL_ACTIVE] = 
        dma_config_hstx.ctrl;
    v_ctx.dma.hstx.ctrl[DMA_CTRL_ACTIVE | DMA_CTRL_RESET] =
        dma_config_hstx.ctrl;

    channel_config_set_chain_to((dma_channel_config*)&v_ctx.dma.hstx.ctrl[DMA_CTRL_BLANK  | DMA_CTRL_RESET], v_ctx.dma.cmdlist_reset.channel);
    channel_config_set_chain_to((dma_channel_config*)&v_ctx.dma.hstx.ctrl[DMA_CTRL_ACTIVE | DMA_CTRL_RESET], v_ctx.dma.cmdlist_reset.channel);

#if 0
    switch (res->xfer_mode) {
        default: break;
        case DVI_XFER_MODE_SRAM_HSTX:
            v_ctx.dma.chain[0].ctrl_pixels  = (c0.ctrl & ~DMA_CH0_CTRL_TRIG_DATA_SIZE_BITS) | pixels_xfer_size;
            v_ctx.dma.chain[1].ctrl_pixels  = (c1.ctrl & ~DMA_CH0_CTRL_TRIG_DATA_SIZE_BITS) | pixels_xfer_size;
            break;
        case DVI_XFER_MODE_SRAM_PIO_HSTX:
            v_ctx.dma.chain[0].ctrl_pixels  = (c0.ctrl & ~(DMA_CH0_CTRL_TRIG_DATA_SIZE_BITS | DMA_CH0_CTRL_TRIG_INCR_READ_BITS)) | pixels_xfer_size;
            v_ctx.dma.chain[1].ctrl_pixels  = (c1.ctrl & ~(DMA_CH0_CTRL_TRIG_DATA_SIZE_BITS | DMA_CH0_CTRL_TRIG_INCR_READ_BITS)) | pixels_xfer_size;
            break;
    }


    // allocate SRAM->PIO DMA channel
    if (res->xfer_mode >= DVI_XFER_MODE_SRAM_PIO_HSTX) {
        dma_channel_config c_pio = dma_channel_get_default_config(res->dma_channels[2]);
        channel_config_set_dreq(&c_pio, PIO_DREQ_NUM(res->pio, res->sm, true));
        dma_channel_configure(
            res->dma_channels[2],
            &c_pio,
            &res->pio->txf[res->sm],    // point to SM TX FIFO
            NULL,
            0,                          // resolve at DMA startup time
            false
        );
        v_ctx.dma.chain[0].src_pixels = 
        v_ctx.dma.chain[1].src_pixels = (uintptr_t)&res->pio->rxf[res->sm];

        v_ctx.dma.pio.channel = res->dma_channels[2];
        v_ctx.dma.pio.hw      = dma_channel_hw_addr(res->dma_channels[2]);
    }
#endif

    // enable DMA IRQ
    dma_irqn_set_channel_enabled(res->dma_irq_line - DMA_IRQ_0, v_ctx.dma.cmdlist_reset.channel, true);
    irq_set_exclusive_handler(res->dma_irq_line, dvi_dma_irq_handler);
    irq_set_priority(res->dma_irq_line, PICO_HIGHEST_IRQ_PRIORITY);
    irq_set_enabled(res->dma_irq_line, true);

    // give DMA the priority
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    // set transfer size
    switch (res->xfer_mode) {
        case DVI_XFER_MODE_SRAM_HSTX:
            v_ctx.active_xfer_count.hstx = v_ctx.timings.h.active / (v_ctx.hstx_input.pixels_per_word * v_ctx.hstx_input.rep_count);
            break;
        case DVI_XFER_MODE_SRAM_PIO_HSTX:
            v_ctx.active_xfer_count.pio  = (v_ctx.timings.h.active / (v_ctx.hstx_input.pixels_per_word * v_ctx.pio.rep_count)) + 1;
            v_ctx.active_xfer_count.hstx =  v_ctx.timings.h.active / (v_ctx.hstx_input.pixels_per_word);
            break;
        default: return 1;
    }
}

// start display output and reset scanline counter
int dvi_start_dma() {
    // setup state machine
    v_ctx.irq_sm.frames   = 0;
    v_ctx.irq_sm.state    = DVI_STATE_FRONT_PORCH;
    v_ctx.irq_sm.scanline = 0;

    // prefill command list
    dvi_fill_command_list();
    dvi_fill_command_list();

    // and start DMA output
    dma_channel_start(v_ctx.dma.cmdlist.channel);
    return 0;
}

// stop DMA
void dvi_stop_dma() {
    if (v_ctx.irq_sm.state == DVI_STATE_IDLE) return;  // already stopped

    // disable IRQ
    //dma_hw->irq_ctrl[v_ctx.irq - DMA_IRQ_0].inte &= ~((1u << v_ctx.dma.chain[1].channel) | (1u << v_ctx.dma.chain[0].channel));
    // clear enable bit (fix errata E5)
    v_ctx.dma.hstx.hw->al1_ctrl             &= ~(DMA_CH0_CTRL_TRIG_EN_BITS);
    v_ctx.dma.cmdlist.hw->al1_ctrl          &= ~(DMA_CH0_CTRL_TRIG_EN_BITS);
    v_ctx.dma.cmdlist_reset.hw->al1_ctrl    &= ~(DMA_CH0_CTRL_TRIG_EN_BITS);
    // abort DMA transfers
    dma_channel_abort(v_ctx.dma.hstx.channel);
    dma_channel_abort(v_ctx.dma.cmdlist.channel);
    dma_channel_abort(v_ctx.dma.cmdlist_reset.channel);
    // acknowledge spurious IRQ
    //dma_hw->intr = 1u << (v_ctx.dma.chain[0].channel | v_ctx.dma.chain[1].channel);
    // enable IRQs
    //dma_hw->irq_ctrl[v_ctx.irq - DMA_IRQ_0].inte |= ((1u << v_ctx.dma.chain[1].channel) | (1u << v_ctx.dma.chain[0].channel));

    // and stop state machine
    v_ctx.irq_sm.state = DVI_STATE_IDLE;
}

