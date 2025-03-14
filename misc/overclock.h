#pragma once
#include <stdint.h>

struct overclock_params_t {
    uint32_t clk_khz;
    struct {
        uint32_t clkdiv;
        uint32_t rxdelay;
    } flash;
    uint32_t  voltage;
    uint32_t  hstx_div;
    uint32_t  textures_from_flash;
};

// overclock the poor pico :) returns true if failed
bool do_overclock(struct overclock_params_t *oc);

// voltage helpers
float voltage_to_float(int voltage);
int float_to_voltage(float voltage);

