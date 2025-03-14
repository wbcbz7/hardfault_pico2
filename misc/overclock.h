#pragma once

struct __packed overclock_params_t {
    uint32_t clk_khz;
    struct {
        uint8_t clkdiv;
        uint8_t rxdelay;
    } flash;
    uint8_t  voltage;
    uint8_t  hstx_div;
};

// overclock the poor pico :) returns true if failed
bool do_overclock(struct overclock_params_t *oc);

