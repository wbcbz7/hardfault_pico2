#include "kucha.h"
#include <stdint.h>
#include "pico.h"

#define KUCHA_SIZE 192*1024
static uint8_t kucha_heap[KUCHA_SIZE];

static uint8_t* kucha_rover = kucha_heap;

void* kucha_alloc(uint32_t size) {
    if ((kucha_rover + size) > (kucha_heap + KUCHA_SIZE)) {
        panic("kucha_alloc(): tried to alloc %d bytes but ran out of memory\n", size);
    }
    uint8_t *ptr = kucha_rover; kucha_rover += size;
    return ptr;
}

void kucha_reset() {
    kucha_rover = kucha_heap;
}

