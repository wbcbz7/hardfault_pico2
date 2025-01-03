#pragma once
#include "pico/util/queue.h"
#include "defs.h"

// core 1 task (speaks for itself)
void core1_task();

// multicore queues for syncrhonization
extern queue_t multicore_queue_msg;   // 0->1
extern queue_t multicore_queue_resp;  // 1->0

typedef struct {
    uint32_t msg;
    union {void *ptr; uint32_t data;};
} queue_msg_t;

// sync messages
enum {
    CORE1_MSG_NOP,
    CORE1_MSG_START_VIDEO,
    CORE1_MSG_START_MUSIC,
    CORE1_MSG_RESTART_MUSIC,
    CORE1_MSG_SET_VIDEO_SCROLL,
    CORE1_MSG_SWAP_BUFFERS,

    CORE1_MSG_EMPTY = -1,
};

enum {
    CORE1_RESP_OK = 0,
    CORE1_RESP_ERROR,
    CORE1_RESP_UNKNOWN_MSG,
    CORE1_RESP_EMPTY = -2,
    CORE1_RESP_TIMEOUT = -1
};

// -------------------------
// SHARED RESOURCES (WARNING: source of potential bugs :D)

// frame buffer
extern uint16_t fb[2][X_RES * Y_RES];
extern uint8_t  fbIdx;
