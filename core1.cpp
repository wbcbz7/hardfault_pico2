
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/structs/sio.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "hardware/pll.h"
#include "hardware/vreg.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "dvi.h"
#include "core1.h"
#include "defs.h"

#define PIXELS_PER_WORD      4

#define MODE_H_SYNC_POLARITY 0
#define MODE_H_FRONT_PORCH   16
#define MODE_H_SYNC_WIDTH    96
#define MODE_H_BACK_PORCH    48
#define MODE_H_ACTIVE_PIXELS 640
#define MODE_H_ACTIVE_PIXELS_PITCH (MODE_H_ACTIVE_PIXELS*4)/PIXELS_PER_WORD

#define MODE_V_SYNC_POLARITY 0
#define MODE_V_FRONT_PORCH   10
#define MODE_V_SYNC_WIDTH    2
#define MODE_V_BACK_PORCH    33
#define MODE_V_ACTIVE_TOTAL  480
#define MODE_V_ACTIVE_LINES  480
#define MODE_V_TOP_BORDER    (MODE_V_ACTIVE_TOTAL-MODE_V_ACTIVE_LINES)/2
#define MODE_V_BOTTOM_BORDER (MODE_V_ACTIVE_TOTAL-MODE_V_ACTIVE_LINES)/2

#define MODE_H_TOTAL_PIXELS ( \
    MODE_H_FRONT_PORCH + MODE_H_SYNC_WIDTH + \
    MODE_H_BACK_PORCH  + MODE_H_ACTIVE_PIXELS \
)
#define MODE_V_TOTAL_LINES  ( \
    MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + \
    MODE_V_BACK_PORCH  + MODE_V_ACTIVE_TOTAL \
)

// ----------------------------------------------------------------------------

// queue queue ueue uu ii aauau
queue_t multicore_queue_msg;   // 0->1
queue_t multicore_queue_resp;  // 1->0

// frame buffer
uint16_t fb[2][X_RES * Y_RES];
uint8_t  fbIdx = 0;

// ----------------------------------------------------------------------------

// HSTX pin layout
#ifdef HSTX_OUT_MURMULATOR2
// Murmulator 2 board
static union dvi_hstx_pin_layout_t hstx_out_pins = {
    .clock_n = 0, .clock_p = 1,
    .lane0_n = 2, .lane0_p = 3,
    .lane1_n = 4, .lane1_p = 5,
    .lane2_n = 6, .lane2_p = 7,
};
#endif

// ----------------------------------------------------------------------------
// audio stuff

#include "i2s.pio.h"

#include "lxmplay.h"
#include "lxmfile.h"
#include "lxm_music.h"

// in sample frames
#ifndef AUDIO_BUFFER_SIZE_LOG2
#define AUDIO_BUFFER_SIZE_LOG2 8
#endif

#define AUDIO_BUFFER_SIZE       (1 << AUDIO_BUFFER_SIZE_LOG2)

// in bytes (stereo 16 bit)
#define AUDIO_BUFFER_SIZE_BYTES (AUDIO_BUFFER_SIZE*4)

// audio sample rate
#define SAMPLE_RATE 44100

// data pins definitions
#define GPIO_I2S_DATA  9
#define GPIO_I2S_CLOCK 10

// audio buffer
static __attribute__((aligned(AUDIO_BUFFER_SIZE_BYTES))) int16_t audiobuf[AUDIO_BUFFER_SIZE*2];

volatile int irq_count = 0, callback_irq_count = 0;
volatile uint32_t audio_buffer_offset;
volatile uint32_t audio_buffer_timestamp;
volatile bool audio_in_callback = false;
int audio_dma_channel;
int audio_dma_irq       = DMA_IRQ_3;
int audio_callback_irq  = FIRST_USER_IRQ;

// LXM context
lxm_context_t lxm_ctx;
#define LXMPLAY_TEST

// render audio
__attribute__((noinline))
void __not_in_flash_func(audio_render)(int16_t *dst, uint32_t frames, uint32_t timestamp) {
    uint32_t t = timestamp;
#if defined(LXMPLAY_TEST)
    lxm_render(&lxm_ctx, dst, frames);
#else
    for (int i = 0; i < frames; i++) {
        uint8_t a = (((t>>0)|(t>>2))|(t>>1))&((t>>8)^(t>>9));
        dst[i*2+0] = dst[i*2+1] = ((int16_t)a << 7);
        t++;
    }
#endif
}

// audio callback interrupt, called with lower priority
void __not_in_flash_func(audio_callback_handler)() {
    // prevent reentrancy issues
    if (audio_in_callback == false) {
        audio_in_callback == true;
        audio_render(audiobuf + audio_buffer_offset, AUDIO_BUFFER_SIZE/2, audio_buffer_timestamp);
        audio_in_callback == false;
    }
    callback_irq_count++;
    irq_clear(audio_callback_irq);
}

// audio interrupt handler
void __not_in_flash_func(audio_dma_interrupt_handler)() {
    if (dma_irqn_get_channel_status(audio_dma_irq - DMA_IRQ_0, audio_dma_channel)) {
        audio_buffer_offset    ^= (AUDIO_BUFFER_SIZE/2)*2;
        audio_buffer_timestamp += (AUDIO_BUFFER_SIZE/2);
        irq_set_pending(audio_callback_irq);
        irq_count++;
        dma_irqn_acknowledge_channel(audio_dma_irq - DMA_IRQ_0, audio_dma_channel);
    }
}

// init video output (res)
int video_init() {
    // init DVI context
    dvi_ctx_init();

    // setup timings
    struct dvi_timings_t timings = {
        .h = {
            .front_porch = MODE_H_FRONT_PORCH,
            .sync        = MODE_H_SYNC_WIDTH,
            .back_porch  = MODE_H_BACK_PORCH,
            .active      = MODE_H_ACTIVE_PIXELS
        },
        .v = {
            .front_porch    = MODE_V_FRONT_PORCH,
            .sync           = MODE_V_SYNC_WIDTH,
            .back_porch     = MODE_V_BACK_PORCH,
            .border_top     = MODE_V_TOP_BORDER,
            .active         = MODE_V_ACTIVE_LINES,
            .border_bottom  = MODE_V_BOTTOM_BORDER,
            .refresh        = 60*1000,
        },
        .flags = DVI_TIMINGS_H_NEG | DVI_TIMINGS_V_NEG
    };
    dvi_set_timings(&timings, DVI_SET_TIMINGS_REFRESH_HBLANK);
    printf("pixel clock = %d.%03d MHz, refresh rate = %d.%03d Hz\n",
        (timings.pixelclock / MHZ),
        (timings.pixelclock % MHZ)/1000,
        timings.v.refresh / 1000,
        timings.v.refresh % 1000
    );
    printf("h: fp % 3d, sync % 3d, bp % 3d, active % 3d, total %d\n",
        timings.h.front_porch, timings.h.sync, timings.h.back_porch, timings.h.active,  timings.h.total
    );
    printf("v: fp % 3d, sync % 3d, bp % 3d, active % 3d, total %d\n",
        timings.v.front_porch, timings.v.sync, timings.v.back_porch, timings.v.active,  timings.v.total
    );

    struct dvi_resources_t dvi_res;
    int pix_fmt   = DVI_PIXEL_FORMAT_XRGB1555;
    int pix_flags = DVI_PIXEL_REP_2;
    
    dvi_get_resources_required(pix_fmt, 0, pix_flags, &dvi_res);
    for (int i = 0; i < dvi_res.dma_channels_num; i++) {
        dvi_res.dma_channels[i] = dma_claim_unused_channel(true);
    }
    dvi_res.dma_irq_line = DMA_IRQ_0;
    
    // allocate and configure free PIO
    dvi_res.pio = pio0;

    dvi_configure_pio(pix_fmt, 0, pix_flags, &dvi_res);
    dvi_set_line_repeat(2);
    dvi_set_framebuffer(fb[0], false);
    dvi_set_pitch(X_RES*2, false);
    dvi_configure_hstx_input(pix_fmt, pix_flags, dvi_res.xfer_mode, NULL);
    dvi_configure_hstx_output(hstx_out_pins);
    dvi_configure_xfer_mode(dvi_res.xfer_mode);
    dvi_init_dma(&dvi_res);
    dvi_start_dma();
    printf("DVI output enabled\n");
    printf("transfer mode: %d\n", dvi_res.xfer_mode);
    return 0;
}

int audio_init() {
    // init audio output
    // init LXM player
    if (lxm_init(&lxm_ctx, 2, SAMPLE_RATE) != 0) {
        printf("error: unable to init lxm!\n");
        while (1);
    }

    if (lxm_load_mem(&lxm_ctx, lxm_music, lxm_music_size) != 0) {
        printf("error: unable to load LXM module\n");
        while (1);
    };

    // prefill audio buffer
    audio_render(audiobuf + 0,                       AUDIO_BUFFER_SIZE/2, 0);
    audio_render(audiobuf + (AUDIO_BUFFER_SIZE/2)*2, AUDIO_BUFFER_SIZE/2, AUDIO_BUFFER_SIZE/2);
    audio_buffer_offset    = (AUDIO_BUFFER_SIZE/2)*2;
    audio_buffer_timestamp = (AUDIO_BUFFER_SIZE/2);
    audio_in_callback      = false;

    // init I2S PIO
    PIO i2s_pio;
    uint i2s_sm; 
    uint i2s_prog_offset;

    // add program
    pio_claim_free_sm_and_add_program(&audio_i2s_program, &i2s_pio, &i2s_sm, &i2s_prog_offset);
    audio_i2s_program_init(i2s_pio, i2s_sm, i2s_prog_offset, GPIO_I2S_DATA, GPIO_I2S_CLOCK);
    uint32_t div = (clock_get_hz(clk_sys) * 4) / SAMPLE_RATE;
    pio_sm_set_clkdiv_int_frac(i2s_pio, i2s_sm, div >> 8u, div & 0xFFu);
    pio_sm_set_enabled(i2s_pio, i2s_sm, true);

    // set pin function
    gpio_set_function(GPIO_I2S_DATA,        (gpio_function_t)((int)GPIO_FUNC_PIO0 + PIO_NUM(i2s_pio)));
    gpio_set_function(GPIO_I2S_CLOCK,       (gpio_function_t)((int)GPIO_FUNC_PIO0 + PIO_NUM(i2s_pio)));
    gpio_set_function(GPIO_I2S_CLOCK + 1,   (gpio_function_t)((int)GPIO_FUNC_PIO0 + PIO_NUM(i2s_pio)));

    audio_dma_channel = dma_claim_unused_channel(true);
    
    dma_channel_config c = dma_channel_get_default_config(audio_dma_channel);
    channel_config_set_dreq(&c, PIO_DREQ_NUM(i2s_pio, i2s_sm, true));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_ring(&c, false, AUDIO_BUFFER_SIZE_LOG2+2);
    
    // hook interrupt
    dma_irqn_set_channel_enabled(audio_dma_irq - DMA_IRQ_0, audio_dma_channel, true);
    irq_add_shared_handler(audio_dma_irq, audio_dma_interrupt_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_exclusive_handler(audio_callback_irq, audio_callback_handler);
    irq_set_priority(audio_dma_irq,      PICO_DEFAULT_IRQ_PRIORITY + 0x10); // allow DVI DMA IRQ preempt audio DMA IRQ
    irq_set_priority(audio_callback_irq, PICO_LOWEST_IRQ_PRIORITY);
    irq_set_enabled(audio_dma_irq, true);
    irq_set_enabled(audio_callback_irq, true);

    // start DMA transfer
    dma_channel_set_write_addr(audio_dma_channel, &i2s_pio->txf[i2s_sm], false);
    dma_channel_set_read_addr(audio_dma_channel, audiobuf, false);
    dma_channel_set_trans_count(
        audio_dma_channel,
        ((AUDIO_BUFFER_SIZE_BYTES/2)/sizeof(uint32_t)) | (DMA_CH0_TRANS_COUNT_MODE_VALUE_TRIGGER_SELF << DMA_CH0_TRANS_COUNT_MODE_LSB),
        false
    );
    dma_channel_hw_addr(audio_dma_channel)->ctrl_trig = channel_config_get_ctrl_value(&c);
    return 0;
}

// ------------------------------------------
// core 1 task
void __scratch_y("") core1_task() {
    printf("core 1 started...\n");

    while (1) {
#if 1
        // wait for message from the queue
        if (!queue_is_empty(&multicore_queue_msg)) {
            queue_msg_t q; queue_remove_blocking(&multicore_queue_msg, &q);
            uint32_t resp = CORE1_RESP_OK;
            switch(q.msg) {
                case CORE1_MSG_NOP: break;
                case CORE1_MSG_START_VIDEO:
                    if (video_init() != 0) resp = CORE1_RESP_ERROR;
                    break;
                case CORE1_MSG_START_MUSIC:
                    if (audio_init() != 0) resp = CORE1_RESP_ERROR;
                    break;
                default: resp = CORE1_RESP_UNKNOWN_MSG; break;
            }
            queue_add_blocking(&multicore_queue_resp, &resp);
        }
#else
        queue_msg_t q; queue_remove_blocking(&multicore_queue_msg, &q);
        uint32_t resp = CORE1_RESP_OK;
        switch(q.msg) {
            case CORE1_MSG_NOP: break;
            default: resp = CORE1_RESP_UNKNOWN_MSG; break;
        }
        queue_add_blocking(&multicore_queue_resp, &resp);
#endif
        // foreground tasks
        sleep_us(100);      // give some chance for core0 to acquire lock
    }
}
