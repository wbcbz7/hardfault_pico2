#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "lxmplay.h"

// internal stuff

static void lxm_update_samples_per_tick(lxm_context_t* ctx) {
    ctx->mixer.spt_reload = ((uint64_t)ctx->mixer.sample_rate << 24ULL) / ctx->header.frame_rate;
}

static void lxm_update_volume(lxm_context_t* ctx, int ch) {
    auto& chan = ctx->channels[ch];
    if (ctx->mixer.out_channels == 2) {
        chan.mixer.vol_l = (int32_t)((int64_t)((int64_t)chan.volume * ctx->mixer.chan_amplify * sqrt(chan.pan / 255.0f)) >> 16);
        chan.mixer.vol_r = (int32_t)((int64_t)((int64_t)chan.volume * ctx->mixer.chan_amplify * sqrt((255 - chan.pan) / 255.0f)) >> 16);
    }
    else {
        chan.mixer.vol_l = (int32_t)(((int64_t)chan.volume * ctx->mixer.chan_amplify) >> 16); // force panning to mono
    }
}

static void lxm_update_pitch(lxm_context_t* ctx, int ch) {
    ctx->channels[ch].mixer.delta.p = ((uint64_t)ctx->channels[ch].pitch * ctx->channels[ch].sample->rate_factor) << 5ULL;
}

int lxm_init(lxm_context_t* ctx, uint32_t out_channels, uint32_t sample_rate)
{
    if ((sample_rate < 8000) || (out_channels != 2)) return 1;

    memset(ctx, 0, sizeof(lxm_context_t));
    ctx->mixer.sample_rate = sample_rate;
    ctx->mixer.out_channels = out_channels;

    return 0;
}

int lxm_free(lxm_context_t* ctx)
{
#if 0
    if (ctx->samples != nullptr) {
        for (int s = 0; s < ctx->header.num_samples; s++) {
            if (ctx->samples[s].data8 != 0) delete[] ctx->samples[s].data8;
        }
        delete[] ctx->samples; ctx->samples = nullptr;
    }
#endif
#if 0
    if (ctx->channels != nullptr) {
        delete[] ctx->channels;
        ctx->channels == nullptr;
    }
#endif
    return 0;
}

#if 0
int lxm_load(lxm_context_t* ctx, const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (f == nullptr) return 1;
    
    fseek(f, 0, SEEK_END);
    uint32_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* data = new uint8_t[fsize];
    fread(data, fsize, 1, f);
    int rtn = lxm_load_mem(ctx, data, fsize);
    delete[] data; // KLUUUUUUDGE!!
    fclose(f);

    return rtn;
}
#endif

static int lxm_volume_lin_to_log(int volume) {
    // volume in 0..65535 range
    if (volume == 0) return 0;
    return (int)(log2(volume) * 8);
}

static int lxm_volume_log_to_lin(int volume) {
    if (volume == 0) return 0;
    return (int)(pow(2.0, ((double)volume / 8.0)));
}

// NB: music data array MUST be resident in memory for the entire playback!
int lxm_load_mem(lxm_context_t* ctx, const void* data, uint32_t size)
{
    if (size == 0) return 1;

    // get header
    lxm_header_t* fhead = (lxm_header_t*)data;
    
    // and validate it
    if (memcmp(fhead->magic, "LXM\x1A", sizeof(fhead->magic))) return 1;
    if (fhead->version.v != 0x0019) return 1;
    if (fhead->num_channels == 0 || fhead->num_channels > 32) return 1;
    if (fhead->num_samples == 0) return 1;

    // copy header
    memcpy(&ctx->header, fhead, sizeof(ctx->header));

    // allocate and copy stream data
    lxm_header_stream_desc_t* streamdesc = (lxm_header_stream_desc_t*)(fhead + 1);
    // alloc ctrl stream
    ctx->stream.data = ((const uint8_t*)data + streamdesc[0].ptr);
    ctx->stream.delay = 1;

    // alloc channel streams
    for (int ch = 0; ch < ctx->header.num_channels; ch++) {
        ctx->channels[ch].stream.data = ((const uint8_t*)data + streamdesc[ch + 1].ptr);
        ctx->channels[ch].stream.delay = 1;
    }

    // check for pitch table
    if (ctx->header.flags & LXM_HEADER_PITCH_TABLE) {
        uint8_t* p = (uint8_t*)streamdesc + (sizeof(lxm_header_stream_desc_t) * (ctx->header.num_channels + 1));
        ctx->pitchtab.size = *(const uint16_t*)p; p += sizeof(uint16_t);
        ctx->pitchtab.data = (const uint16_t*)p;
    }

    // parse samples
    lxm_sample_t* fsmp = (lxm_sample_t*)((uint8_t*)data + fhead->samples_ptr);
    for (int i = 0; i < fhead->num_samples; i++) {
        // copy header
        memcpy(&ctx->samples[i].header, fsmp, sizeof(lxm_sample_t));

        // calculate rate factor
        ctx->samples[i].rate_factor = ((int64_t)fsmp->sample_rate << 8) / ctx->mixer.sample_rate;

        // copy sample data
        uint32_t bytes_per_sample = (fsmp->flags & LXM_SAMPLE_FORMAT_MASK) == LXM_SAMPLE_16BIT ? 2 : 1;
        ctx->samples[i].data8 = ((const int8_t*)data + fsmp->ofs_data);
        fsmp++;
    }

    // set mixer parameters
    lxm_update_samples_per_tick(ctx);
    ctx->mixer.spt_count    = 0;
    ctx->mixer.chan_amplify = ((ctx->header.num_channels >= 8) ? (1 << 17) : (1 << 16)) / (ctx->header.num_channels);

    // rewind to start
    lxm_rewind(ctx);

    // done :)
    return 0;
}

int lxm_loop(lxm_context_t* ctx) {
    // control stream
    ctx->stream.ptr  = ctx->loop.data;
    ctx->pos.frame   = ctx->loop.frames;
    ctx->pos.samples = ctx->loop.samples;
    ctx->stream.reload = ctx->stream.delay = 1;

    // channel streams
    for (int ch = 0; ch < ctx->header.num_channels; ch++) {
        // init stack
        ctx->channels[ch].stack_pos = 0;
        ctx->channels[ch].stream.samples_to_play = UINT32_MAX;
        ctx->channels[ch].stream.ptr = ctx->channels[ch].stream.loop;
        ctx->channels[ch].stream.delay = ctx->channels[ch].stream.reload = 1;
    }

    return 0;
}

void lxm_pop_stack(lxm_channel_context_t* chctx) {
    lxm_stream_stack_t* st          = chctx->stack + (--chctx->stack_pos);
    chctx->stream.ptr               = st->stream;
    chctx->stream.samples_to_play   = st->samples_to_play;
}

void lxm_push_stack(lxm_channel_context_t* chctx) {
    lxm_stream_stack_t* st  = chctx->stack + chctx->stack_pos;
    st->stream              = chctx->stream.ptr;
    st->samples_to_play     = chctx->stream.samples_to_play;
    chctx->stack_pos++;
}

int lxm_rewind(lxm_context_t* ctx) {
    // set loop poisiotns
    ctx->loop.data = ctx->stream.data;
    ctx->loop.frames = -1;      // TODO: checkme
    ctx->loop.samples = 0;      // TODO: checkme

    // set control stream
    ctx->stream.ptr = ctx->stream.data;
    ctx->stream.delay = ctx->stream.reload = 1;
    ctx->mixer.spt_count = 0;
    ctx->pos.frame = -1; ctx->pos.order = ctx->pos.samples = 0;

    // set sample 0 for all channels to guard against broken streams
    for (int ch = 0; ch < ctx->header.num_channels; ch++) {
        ctx->channels[ch].stream.ptr = ctx->channels[ch].stream.loop = ctx->channels[ch].stream.data;
        ctx->channels[ch].stream.delay = ctx->channels[ch].stream.reload = 1;
        ctx->channels[ch].sample = ctx->samples + 0;
        ctx->channels[ch].volume = 0;
        ctx->channels[ch].pitch  = 0;
        lxm_update_pitch(ctx, ch);
        lxm_update_volume(ctx, ch);
    }

    return 0;
}

static void lxm_update_ofs(lxm_context_t* ctx, int ch, int ofs) {
    if (ofs >= ctx->channels[ch].sample->header.length) {
        // cut the sample if the position is off-bounds
        ctx->channels[ch].volume = 0;
        lxm_update_volume(ctx, ch);
        ctx->channels[ch].mixer.pos.p = 0;
        ctx->channels[ch].mixer.stopped = true;
    }
    else {
        ctx->channels[ch].mixer.pos.i = ofs;
        ctx->channels[ch].mixer.pos.f = 0;
        ctx->channels[ch].mixer.stopped = false;
    }
}
static const uint8_t* lxm_parse_set_mask(lxm_context_t *ctx, int ch, const uint8_t *data, int valmask) {
    if (valmask & LXM_STREAM_SET_SAMPLE) {
        ctx->channels[ch].sample = ctx->samples + *data; data += 1;
        ctx->channels[ch].mixer.pos.p = 0;
        ctx->channels[ch].mixer.stopped = false;
        ctx->channels[ch].mixer.play_backwards = false;
    }
    if (valmask & LXM_STREAM_SET_PITCH) {
        if ((*(data + 0) & 0x80) && (ctx->header.flags & LXM_HEADER_PITCH_TABLE)) {
            // get from pitch table
            ctx->channels[ch].pitch = ctx->pitchtab.data[*(data + 0) & 0x7F];    data += 1;
        }
        else {
            // get from event data
            ctx->channels[ch].pitch = (*(data + 0) << 8) | (*(data + 1) & 0xFF); data += 2;
        }
        // set pitch
        lxm_update_pitch(ctx, ch);
    }
    if (valmask & LXM_STREAM_SET_PAN) {
        ctx->channels[ch].pan = *data++;
        lxm_update_volume(ctx, ch);
    }
    if (valmask & LXM_STREAM_SET_OFS) {
        lxm_update_ofs(ctx, ch, (*data++ << 8));
        ctx->channels[ch].mixer.play_backwards = false;
    }

    return data;
}

// get and parse delay
uint32_t lxm_set_delay(const uint8_t** data) {
    uint32_t delay = 0;
    if (**data == LXM_STREAM_DELAY_INT32) {
        delay = (
            (*(*data + 1) <<  0) |
            (*(*data + 2) <<  8) |
            (*(*data + 3) << 16) |
            (*(*data + 4) << 24)
        );
        *data += 5;
    }
    else if (**data == LXM_STREAM_DELAY_INT16) {
        delay = (
            (*(*data + 1) << 0) |
            (*(*data + 2) << 8)
            );
        *data += 3;
    }
    else if ((**data & 0xF0) == LXM_STREAM_DELAY_INT12) {
        delay = ((**data & 0x0F) << 8) | (*(*data + 1));
        *data += 2;
    }
    else if ((**data & 0xF0) == LXM_STERAM_DELAY_SHORT) {
        delay = (**data & 0xF) + 1;
        (*data)++;
    }
    return delay;
}

int lxm_tick(lxm_context_t* ctx) {
    int ch = 0;
    int rtn = 0;
    uint32_t newdelay = 0;

    // process control stream
    const uint8_t* data = ctx->stream.ptr;
    bool isRun = true;
    if (--ctx->stream.delay == 0) {
        while (isRun) {
            // check for common stuff
            switch (*data) {
                // end of stream ewind everything
            case LXM_STREAM_END:
                lxm_loop(ctx);
                isRun = false;
                return -1;
                // just an NOP, break
            case LXM_STREAM_NEW_ORDER:
            case LXM_STREAM_NOP:
                data++;
                break;
            case LXM_STREAM_LOOP:
                // save loop point
                ctx->loop.frames = ctx->pos.frame;
                ctx->loop.samples = ctx->pos.samples;
                ctx->loop.data = data;
                data++;
                break;
                // set new frame rate
            case LXM_STREAM_SET_FRAME_RATE:
                ctx->header.frame_rate = *(const uint16_t*)(data + 1); data += 3;
                // recalculate delay reload
                lxm_update_samples_per_tick(ctx);
                break;
            case LXM_STREAM_END_FRAME:
                // end of frame - special case here
                data++;
                isRun = false;
                break;

            default:
                // test for delay
                newdelay = lxm_set_delay(&data);
                if (newdelay > 0) {
                    ctx->stream.reload = newdelay;
                }
                else {
                    // unknown token, break the stream!
                    printf("unknown token %02X at %08X!\n", *data, data - ctx->stream.data);
                    return 1;
                }
            }
        }
        ctx->stream.delay = ctx->stream.reload;
        ctx->stream.ptr = data;
    }

    // process channel stream
    lxm_channel_context_t* chctx = ctx->channels;
    for (int ch = 0; ch < ctx->header.num_channels; ch++) {
        const uint8_t* data = chctx->stream.ptr;
        bool isRun = true;
        if (--chctx->stream.delay == 0) {
            while (isRun) {
                // parse setting parameters for the channel
                if ((*data & 0x80) == LXM_STREAM_VOLUME) {
                    isRun = false;
                    //chctx->volume = lxm_volume_log_to_lin(*data & 0x7F);
                    chctx->volume = (*data & 0x7F) << 9;
                    lxm_update_volume(ctx, ch);
                    data++;
                    chctx->updated = true;
                    continue;
                }
                if ((*data & 0xE0) == LXM_STREAM_SET) {
                    int valmask = *data & 0x0F;
                    if (*data & LXM_STREAM_SET_END) isRun = false;
                    data++;
                    data = lxm_parse_set_mask(ctx, ch, data, valmask);
                    chctx->updated = true;
                    continue;
                }
                if ((*data & 0xF0) == LXM_STREAM_RETRIG) {
                    int valmask = *data & 0x07;
                    if (*data & LXM_STREAM_RETRIG_END) isRun = false;
                    data++;
                    data = lxm_parse_set_mask(ctx, ch, data, valmask);
                    ctx->channels[ch].mixer.play_backwards = false;
                    lxm_update_ofs(ctx, ch, 0);
                    chctx->updated = true;
                    continue;
                }
                if ((*data & 0xF0) == LXM_STREAM_BACKREF) {
                    // back reference, nested call :)
                    int distance = ((*(data + 0) & 0x0F) << 8) | (*(data + 1));
                    int frames_to_play = *(data + 2);
                    chctx->stream.ptr = data + 3;
                    lxm_push_stack(chctx);
                    data -= distance;
                    chctx->stream.samples_to_play = frames_to_play; // hack?
                    continue;
                }
                // parse other commands
                switch (*data) {
                case LXM_STREAM_NEW_ORDER:
                case LXM_STREAM_NOP:
                    data++;
                    break;
                case LXM_STREAM_LOOP:
                    // save loop point
                    chctx->stream.loop = data;
                    data++;
                    break;
                case LXM_STREAM_END_FRAME:
                    // end of frame - special case here
                    data++;
                    isRun = false;
                    break;
                case LXM_STREAM_END:
                    // end of current stream, delay forever
                    chctx->stream.reload = -1;
                    isRun = false;
                    break;

                default:
                    // test for delay
                    newdelay = lxm_set_delay(&data);
                    if (newdelay > 0) {
                        chctx->stream.reload = newdelay;
                    }
                    else {
                        // unknown token, break the stream!
                        printf("unknown token %02X at %08X!\n", *data, data - ctx->stream.data);
                        return 1;
                    }
                }
            }

            // reload delay counter
            chctx->stream.delay = chctx->stream.reload;

            // decrement samples to play counter
            if (--chctx->stream.samples_to_play == 0) {
                // pop context from the stack
                do lxm_pop_stack(chctx); while (--chctx->stream.samples_to_play == 0);
            }
            else {
                // save data pointer
                chctx->stream.ptr = data;
            }
        }
        chctx++;
    }

    return rtn;
}

// ------------------------------
// render engine

template <typename T>
static void lxm_render_channel(lxm_context_t* ctx, int ch, int16_t* buf, int32_t samples) {
    auto& chan    = ctx->channels[ch];
    auto  smpctx  = chan.sample;
    const T* smp  = (const T*)chan.sample->data8;

    do {
        // fetch sample
        // TODO: interpolation?
        int64_t smp_fetched = ((int64_t)*(smp + chan.mixer.pos.i)) << (16 - (CHAR_BIT * sizeof(T)));

#if 1
        // write
        *(buf + 0) += (smp_fetched * chan.mixer.vol_l) >> 16;
        *(buf + 1) += (smp_fetched * chan.mixer.vol_r) >> 16;
        buf += 2;
#else
        // write
        if (ctx->mixer.out_channels == 1) {
            // mono
            *buf += (smp_fetched * chan.mixer.vol_l) >> 16;
            buf++;
        }
        else {
            *(buf + 0) += (smp_fetched * chan.mixer.vol_l) >> 16;
            *(buf + 1) += (smp_fetched * chan.mixer.vol_r) >> 16;
            buf += 2;
        }
#endif

        // advance
        if (chan.mixer.play_backwards) {
            chan.mixer.pos.p -= chan.mixer.delta.p;
        }
        else {
            chan.mixer.pos.p += chan.mixer.delta.p;
        }

        // advance, check for loop condition
        switch (smpctx->header.flags & LXM_SAMPLE_LOOP_MASK) {
        case LXM_SAMPLE_ONESHOT:
            if (chan.mixer.pos.i >= smpctx->header.length) {
                // end of channel!
                chan.mixer.stopped = true;
                return;
            }; 
        case LXM_SAMPLE_LOOP_FORWARD:
            if (chan.mixer.pos.i >= smpctx->header.length) 
                chan.mixer.pos.i -= (smpctx->header.length - smpctx->header.loop_start);
            break;
        case LXM_SAMPLE_LOOP_BIDIR:
            // ugh
            if (chan.mixer.play_backwards) {
                if (chan.mixer.pos.i < smpctx->header.loop_start) {
                    // TODO: "reflect" pos overflow
                    chan.mixer.play_backwards = false;
                    chan.mixer.pos.p += (((int64_t)smpctx->header.loop_start << 32LL) - chan.mixer.pos.p) << 1;
                }
            }
            else {
                if (chan.mixer.pos.i >= smpctx->header.length) {
                    // TODO: "reflect" pos overflow
                    chan.mixer.play_backwards = true;
                    chan.mixer.pos.p -= (chan.mixer.pos.p - ((int64_t)smpctx->header.length << 32LL)) << 1;
                }
            }
            break;
        default:
            break;

        }

    } while (--samples);
}

static void lxm_render_frame(lxm_context_t* ctx, int16_t* buf, int32_t samples) {
    if (samples == 0) return;;

    for (int ch = 0; ch < ctx->header.num_channels; ch++) {
        if (ctx->channels[ch].mixer.stopped == false) switch (ctx->channels[ch].sample->header.flags & LXM_SAMPLE_FORMAT_MASK) {
        case LXM_SAMPLE_8BIT:
            lxm_render_channel<int8_t>(ctx, ch, buf, samples);
            break;
        case LXM_SAMPLE_16BIT:
            lxm_render_channel<int16_t>(ctx, ch, buf, samples);
            break;
        default:
            break;

        }
        
    }
}

#define min(a, b) ((a) < (b) ? (a) : (b))

// returns sample frames rendered or 0 if none or error
int lxm_render(lxm_context_t* ctx, int16_t* buf, int32_t count) {
    if ((buf == nullptr) || (count == 0)) return 1;

    // prepare buffer
    memset(buf, 0, sizeof(int16_t) * count * ctx->mixer.out_channels);
    int32_t total_count = count;

    // render each tick
    int16_t* p = buf;
    while (total_count > 0) {
        // check for next tick
        if (ctx->mixer.spt_count < (1 << 16)) {
            if (lxm_tick(ctx) == -1) {
                ctx->pos.looped = true;  break;
            }
            ctx->mixer.spt_count += ctx->mixer.spt_reload;
            ctx->pos.frame++;
        }

        // render current tick
        int32_t current_count = min(total_count, ctx->mixer.spt_count >> 16);
        lxm_render_frame(ctx, p, current_count);
        total_count -= current_count;
        p += current_count * ctx->mixer.out_channels;
        ctx->mixer.spt_count -= (current_count << 16);
    }

    ctx->pos.samples += (count - total_count);
    return (count - total_count);
}

