
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
#include "defs.h"
#include "core1.h"

// parts include
#include "parts/linetunnel.h"

// LED blinker in case of errors
void blink_led_hang() {
    while(1) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(200);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(200);
    }
}

// ----------------------------------------------------------------------------
// post request for the queue
void queue_post_msg(uint32_t msg, void *ptr) {
    queue_msg_t q = {.msg = msg, .ptr = ptr};
    queue_add_blocking(&multicore_queue_msg, &q);
}

// fetch response from queue
uint32_t queue_get_resp(uint32_t timeout_us) {
    if (timeout_us != 0) {
        absolute_time_t deadline = time_us_64() + timeout_us;
        while ((time_us_64() <= deadline) && queue_is_empty(&multicore_queue_resp));
        if (queue_is_empty(&multicore_queue_resp)) return CORE1_RESP_TIMEOUT;
    }
    uint32_t rtn;
    queue_remove_blocking(&multicore_queue_resp, &rtn);
    return rtn;
}

// ----------------------------------------------------------------------------
// Main program

int main(void) {
    // configure the almighty debug LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // init stdio
    stdio_init_all();
    printf("-------------------------------\n");
    
    // bump up RP2350 voltage a bit
    vreg_set_voltage(VREG_VOLTAGE_1_25);

    printf("target sysclk = %d kHz\n", (MODE_PIXEL_CLOCK*5*CLK_SYS_MUL)/1000);
    // configure PLL for required pixel clock
    if (!set_sys_clock_khz((MODE_PIXEL_CLOCK*5*CLK_SYS_MUL)/1000, false)) {
        printf("fatal: unable to configure sysclk!\n");
        blink_led_hang();
    };
    // configure as usual
    clock_configure_int_divider(clk_hstx, CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS, 0, clock_get_hz(clk_sys), CLK_SYS_MUL);

    // reinit stdio
    stdio_init_all();
    printf("sysclk switch success\n");

    // alloc queues
    queue_init(&multicore_queue_msg,  sizeof(queue_msg_t), 2);
    queue_init(&multicore_queue_resp, sizeof(uint32_t), 2);

    // start core1
    printf("starting core 1...\n");
    multicore_launch_core1(core1_task);

    // test echo response
    queue_post_msg(CORE1_MSG_NOP, 0);
    uint32_t resp = queue_get_resp(20*1000);
    if (resp != 0) {
        printf("core 1 ping fail: rtn = %d\n", resp);
        blink_led_hang();
    } else printf("core 1 ping success\n");

    // start video
    queue_post_msg(CORE1_MSG_START_VIDEO, 0);
    if ((resp = queue_get_resp(0)) != 0) {
        printf("unable to start video: resp = %d\n", resp);
        blink_led_hang();
    }

    // start audio
    queue_post_msg(CORE1_MSG_START_MUSIC, 0);
    if ((resp = queue_get_resp(0)) != 0) {
        printf("unable to start audio: resp = %d\n", resp);
        blink_led_hang();
    }

    linetunnel_run();
}
