
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/structs/sio.h"
#include "hardware/interp.h"
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

#include <stdio_async_uart.h>
#include "textures/endpic.h"

// parts include
#include "parts/linetunnel.h"
#include "parts/test3d.h"
#include "parts/bmpdist.h"
#include "parts/tunnel.h"
#include "parts/bgmap.h"

#include <overclock.h>
#include <fbstuff.h>
#include <argb.h>
#include <timer.h>
#include "lxmplay.h"

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

// default overclocking parameters
struct overclock_params_t ocparms = {
    .clk_khz = (MODE_PIXEL_CLOCK*5*CLK_SYS_MUL)/1000,
    .flash = {
        .clkdiv  = 3,
        .rxdelay = 3
    },
    .voltage  = VREG_VOLTAGE_1_40,
    .hstx_div = CLK_SYS_MUL,
    .textures_from_flash = 0,
};

void set_config() {
    printf("Enter system clock freqency [current - %d.%02d MHz]: ", ocparms.clk_khz / 1000, (ocparms.clk_khz % 1000) / 10);  fflush(stdout);
    float new_sysclk;
    if ((scanf("%f", &new_sysclk) == 1) && new_sysclk > 0) {
        ocparms.clk_khz = (uint32_t)((new_sysclk * MHZ) / 1000);
    }
    printf("%d.%02d\n", ocparms.clk_khz / 1000, (ocparms.clk_khz % 1000) / 10);

    printf("Enter QMI Flash clock divisor [current - %d]: ", ocparms.flash.clkdiv); fflush(stdout);
    scanf("%u", &ocparms.flash.clkdiv);
    if (ocparms.flash.clkdiv <= 0) ocparms.flash.clkdiv = (uint32_t)(new_sysclk / 133333.0f);
    printf("%d\n", ocparms.flash.clkdiv);

    printf("Enter QMI Flash RX delay [current - %d]: ", ocparms.flash.rxdelay); fflush(stdout);
    scanf("%u", &ocparms.flash.rxdelay);
    printf("%d\n", ocparms.flash.rxdelay);
    if (ocparms.flash.rxdelay > ocparms.flash.clkdiv) printf("warning: RX delay must be less clock divisor!\n");

    printf("Enter voltage [current - %.2f]: ", voltage_to_float(ocparms.voltage)); fflush(stdout);
    float tempvoltage;
    if ((scanf("%f", &tempvoltage) == 1) && tempvoltage > 0) {
        ocparms.voltage = float_to_voltage(tempvoltage);
    }
    printf("%.2f\n", tempvoltage);
    if (ocparms.voltage > VREG_VOLTAGE_1_50) printf("WARNING: VOLTAGE > 1.50 V!\n");

    // calculate HSTX divisor to match the refresh rate
    ocparms.hstx_div = ((float)ocparms.clk_khz / (5*MODE_PIXEL_CLOCK/KHZ));
    uint32_t hstx_clk = ocparms.clk_khz / ocparms.hstx_div;
    printf("HSTX divisor = %d, HSTX clock = %d.%02d MHz\n", 
        ocparms.hstx_div, hstx_clk / 1000, (hstx_clk % 1000) / 10
    );

    printf("Select texture storage:\n");
    printf("0 - from SRAM  (more load on CPU/SRAM)\n");
    printf("1 - from flash (more load on flash)\n");
    int a = 0; scanf("%u", &a);
    if (a == 1) ocparms.textures_from_flash = 1;
    printf("---------------\n");
}

void print_oc_settings(struct overclock_params_t *oc) {
    uint32_t flashclk = oc->clk_khz / oc->flash.clkdiv;
    uint32_t hstxclk  = oc->clk_khz / oc->hstx_div;
    printf("sysclk = %d.%02d MHz, QMI flash divisor = %d (SCLK = %d.%02d MHz), RX delay = %d, voltage = %.2fV, HSTX clock = %d.%02d MHz\n",
        oc->clk_khz / 1000, (oc->clk_khz % 1000) / 10,
        oc->flash.clkdiv, flashclk / 1000, (flashclk % 1000) / 10, oc->flash.rxdelay,
        voltage_to_float(ocparms.voltage),
        hstxclk / 1000, (hstxclk % 1000) / 10
    );
}

// ----------------------------------------------------------------------------
// Main program

int main(void) {
    // reset core1 and wait a moment to prevent issues after flashing
    multicore_reset_core1();
    sleep_ms(500);

    // configure the almighty debug LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // init stdio
    stdio_init_all();
    stdio_async_uart_init_full(uart0, 115200, PICO_DEFAULT_UART_TX_PIN, PICO_DEFAULT_UART_RX_PIN);
    printf("-------------------------------\n");
    printf("RP2350 hard_faulty ducky overclocking test - artemka 15.o3.2o25\n");
    print_oc_settings(&ocparms);
    
    int led_state = 0;
    bool keypress = false;
    {
        printf("press any key for configuration or wait for 5 seconds for auto start\n");
        while (time_us_32() < 5*1000*1000) {
            sleep_ms(200); putc('.', stdout); fflush(stdout);
            led_state ^= 1;
            gpio_put(PICO_DEFAULT_LED_PIN, led_state);
            if (stdio_getchar_timeout_us(0) >= 0) {
                keypress = true; break;
            }
        }
        printf("\n");
        if (keypress) set_config();
    }
    print_oc_settings(&ocparms);
    fflush(stdout);
    sleep_ms(100);

    // overclock!
    if (do_overclock(&ocparms)) {
        printf("unable to overclock!");
        blink_led_hang();
    };

    // reinit stdio
    stdio_init_all();
    stdio_async_uart_init_full(uart0, 115200, PICO_DEFAULT_UART_TX_PIN, PICO_DEFAULT_UART_RX_PIN);
    
    printf("sysclk switch success\n");

    // claim some of used HW
    interp_claim_lane_mask(interp0, 3);
    interp_claim_lane_mask(interp1, 3); // claim both interpolators since they are used for texture mapping

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
    printf("video start success\n");

#if 0
    // clear framebuffer, wait 4 seconds to wakeup
    fb_fill_a(fb[fbIdx], argb_to_555(32, 8, 8), X_RES*Y_RES);
    sleep_ms(1*1000);
    fb_fill_a(fb[fbIdx], argb_to_555(8, 8, 32), X_RES*Y_RES);
    sleep_ms(1*1000);
    fb_fill_a(fb[fbIdx], argb_to_555(32, 32, 32), X_RES*Y_RES);
    sleep_ms(1*1000);
#endif
    
#if 1
    // start audio
    queue_post_msg(CORE1_MSG_START_MUSIC, 0);
    if ((resp = queue_get_resp(0)) != 0) {
        printf("unable to start audio: resp = %d\n", resp);
        blink_led_hang();
    }
#endif

#if 0
    fb_fill_a(fb[fbIdx], argb_to_555(16, 16, 48), X_RES*Y_RES);
    fb_fill_a(fb[fbIdx^1], argb_to_555(16, 16, 48), X_RES*Y_RES);
    while (lxm_current_frame() < (3*16));
#endif

#if 0
    bmpdist_init();
    linetunnel_init();
    ftimer_set(0.0);
    linetunnel_run();
    linetunnel_done();
#endif
#if 0
    ftimer_set(0.0);
    bmpdist_run();
    bmpdist_done();
#endif
#if 0
    tunnel_init();
    ftimer_set(0.0);
    tunnel_run();
    tunnel_done();
#endif
#if 1
    test3d_init();
    ftimer_set(0.0);
    test3d_run();
    test3d_done();
#endif
    printf("end of demo. :p\n");
    blink_led_hang();
}
