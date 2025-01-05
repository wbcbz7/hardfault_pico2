#pragma once
#include <stdint.h>
#include "lxmfile.h"

// forward declarations
struct lxm_channel_context_t;
struct lxm_sample_context_t;

typedef union lxm_frac_t {
    struct {
        uint32_t f;
        int32_t  i;
    };
    int64_t p;
} lxm_frac_t;

typedef struct lxm_stream_stack_t {
    const uint8_t*  stream;
    uint32_t        samples_to_play;
} lxm_stream_stack_t;

enum {
    LXMPLAY_MAX_STACK_DEPTH = 4,
    LXMPLAY_MAX_CHANNELS    = 4,
    LXMPLAY_MAX_SAMPLES     = 16,
};

typedef struct lxm_channel_context_t {
    lxm_sample_context_t* sample;   // current sample

    uint32_t        pitch;          // current pitch
    uint32_t        volume;         // current volume
    uint32_t        pan;            // current panning

    // mixer context
    struct {
        int32_t     vol_l, vol_r;   // 16.16fx

        lxm_frac_t  pos;            // 32.32fx
        lxm_frac_t  delta;          // 32.32fx
        lxm_frac_t  delta_cur;      // 32.32fx - current delta

        bool        play_backwards; 

        int         stopped;
    } mixer;

    // stream stack
    lxm_stream_stack_t      stack[LXMPLAY_MAX_STACK_DEPTH];
    int                     stack_pos;

    // stream data
    struct {
        uint32_t            samples_to_play;
        uint32_t            delay;
        uint32_t            reload;

        const uint8_t*      data;
        const uint8_t*      ptr;
        const uint8_t*      loop; // if active
    } stream;

    bool            updated;
} lxm_channel_context_t;

typedef struct lxm_sample_context_t {
    lxm_sample_t    header;

    // mixer.delta = (channel.pitch * sample.rate) / global.rate = channel.pitch * rate_factor;
    uint32_t        rate_factor;    // 16.16fx

    // data unions
    union {
        const int8_t*     data8;
        const int16_t*    data16;
    };
} lxm_sample_context_t;

typedef struct lxm_mixer_context_t {
    // next row delay count/reload
    int32_t spt_count;      // 16.16fx
    int32_t spt_reload;     // 16.16fx

    uint32_t out_channels;  // 1 or 2
    uint32_t sample_rate;   // sample rate

    // amplify
    uint32_t chan_amplify;  // 16.16fx
} lxm_mixer_context_t;

typedef struct lxm_context_t {
    lxm_header_t            header;

    // sample contexts
    lxm_sample_context_t    samples[LXMPLAY_MAX_SAMPLES];

    // channel context
    lxm_channel_context_t   channels[LXMPLAY_MAX_CHANNELS];

    // mixer context
    lxm_mixer_context_t     mixer;

    // pitch table data
    struct {
        int                 size;
        const uint16_t*     data;
    } pitchtab;

    // control stream data
    struct {
        uint32_t            delay;
        uint32_t            reload;

        const uint8_t*      data;
        const uint8_t*      ptr;
    } stream;
    
    // position data 
    struct {
        uint32_t            order;
        uint32_t            frame;
        uint64_t            samples;

        uint32_t            looped;
    } pos;

    // loop data
    struct {
        const uint8_t*      data;
        uint32_t            frames;
        uint64_t            samples;
    } loop;

} lxm_context_t;

// init context
int lxm_init(lxm_context_t* ctx, uint32_t out_channels, uint32_t sample_rate);

// free context
int lxm_free(lxm_context_t* ctx);

// load from file
int lxm_load(lxm_context_t* ctx, const char *filename);

// load from memory
int lxm_load_mem(lxm_context_t* ctx, const void *data, uint32_t size);

// reset to start
int lxm_rewind(lxm_context_t* ctx);

// play one tick
int lxm_tick(lxm_context_t* ctx);

// render int16_t stereo samples to buffer
// returns sample frames rendered or 0 if error
int lxm_render(lxm_context_t* ctx, int16_t* buf, int32_t count);

// return current frame counter
int lxm_current_frame();

