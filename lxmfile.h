#pragma once
#include <stdint.h>

#pragma pack(push, 1)

typedef struct lxm_header_stream_desc_t {
    uint32_t    ptr;        // offset to data stream
    uint32_t    size;       // stream data size in bytes
} lxm_header_stream_desc_t;

enum {
    LXM_HEADER_LOG_VOLUME  = (1 << 0),
    LXM_HEADER_PITCH_TABLE = (1 << 1),
};

typedef struct lxm_header_t {
    char        magic[4];       // "LXM\x1A"
    union {
        struct {
            uint8_t minor;
            uint8_t major;
        };
        uint16_t v;
    } version;
    uint16_t    flags;              // reserved
    uint16_t    frame_rate;         // in Hz, 8.8fx
    uint16_t    num_samples;
    uint8_t     num_channels;       // 0..32
    uint8_t     callstack_depth;    // max nested backrefs   
    uint16_t    amplification;      // 8.8fx, for software mixer
    uint32_t    samples_ptr;        // offset to sample descriptors

    //lxm_header_stream_desc_t stream[num_channels + 1]; // first is control stream

    /*
    if (flags & LXM_HEADER_PITCH_TABLE) {
        uint16_t pitchtab_size;     // 0..127
        uint16_t pitchtab[pitchtab_size];
    }
    */
} lxm_header_t;

// sample descriptors, stored sequentially
enum {
    LXM_SAMPLE_8BIT = (0 << 0),
    LXM_SAMPLE_16BIT = (1 << 0),
    LXM_SAMPLE_FORMAT_MASK = (7 << 0),

    LXM_SAMPLE_ONESHOT = (0 << 3),
    LXM_SAMPLE_LOOP_FORWARD = (1 << 3),
    LXM_SAMPLE_LOOP_BIDIR = (2 << 3),
    LXM_SAMPLE_LOOP_MASK = (3 << 3),
    LXM_SAMPLE_LOOP_SHIFT = 3,
};

typedef struct lxm_sample_t {
    uint16_t    flags;
    uint16_t    compression;    // currently 0
    uint32_t    sample_rate;    // in hz, 24.8fx
     int32_t    length;         // in samples, sustain loops not supported
     int32_t    loop_start;     // in samples
    uint32_t    ofs_data;       // offset in file

    // optional, contains 0 if ignored
    uint32_t    max_sample_rate;    // maximum sample rate occured in the file, used for target device sample rate optimization (emu8k :)
    uint32_t    opt_data;           // sample optimization data (priority, etc)
    uint32_t    reserved;
} lxm_sample_t;

// LXM v0 stream data:
enum {
    LXM_STREAM_END_FRAME        = 0xFF,     // end of frame, set channel 0
    LXM_STREAM_END              = 0xFE,     // end of stream, stop here or loop to LXM_STREAM_LOOP stream point
    LXM_STREAM_NOP              = 0xFD,
    LXM_STREAM_NEW_ORDER        = 0xFC,     // nop, marks new order
    LXM_STREAM_SET_FRAME_RATE   = 0xFB,     // word rate (as in lxm_header_t::frame_rate)
    LXM_STREAM_LOOP             = 0xFA,     // set loop point here

    // delay commands
    LXM_STREAM_DELAY_INT32      = 0xF9,     // dword delay
    LXM_STREAM_DELAY_INT16      = 0xF8,     // word  delay
    LXM_STREAM_DELAY_INT12      = 0xD0,     // D0..DF - 0..4095 frames delay (hibyte in low 4 bits of command)
    LXM_STERAM_DELAY_SHORT      = 0xC0,     // C0..CF - 1..16 frames delay
    
    // back reference 
    LXM_STREAM_BACKREF          = 0xE0,     // E0..EF - word backrefpos (12 bit), byte frames

    // setter commands
    LXM_STREAM_VOLUME           = 0x00,     // 00..7F - volume is 7bit log compressed, end the frame
    LXM_STREAM_SET              = 0x80,     // 80..9F - set all params
    LXM_STREAM_RETRIG           = 0xA0,     // A0..AF - retrig note (ofs = 0), set all params
    LXM_STREAM_SET_EXT          = 0xB0,     // B0..BF - reserved

    // channel flags
    LXM_STREAM_SET_SAMPLE       = (1 << 0), // byte
    LXM_STREAM_SET_PITCH        = (1 << 1), // byte/word pitch/note compressed (see below)
    LXM_STREAM_SET_PAN          = (1 << 2), // byte left 00 .. 80 .. FF right
    LXM_STREAM_SET_OFS          = (1 << 3), // byte (sample_pos >> 8)

    LXM_STREAM_RETRIG_END       = (1 << 3), // end the frame
    LXM_STREAM_SET_END          = (1 << 4), // end the frame
};

#pragma pack(pop)
 
