#include "hardware/structs/qmi.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "overclock.h"

bool do_overclock(struct overclock_params_t *oc) {
    // set RP2350 voltage
    vreg_disable_voltage_limit();
    vreg_set_voltage(oc->voltage);
    sleep_ms(1);

    // set QMI flash timings
    qmi_mem_hw_t *qmidev = &qmi_hw->m[0];

    uint32_t timing = qmidev->timing;
    if (oc->flash.clkdiv  != 0xFF) 
        timing = (timing & ~QMI_M0_TIMING_CLKDIV_BITS) |
                 ((oc->flash.clkdiv  << QMI_M0_TIMING_CLKDIV_LSB)  & QMI_M0_TIMING_CLKDIV_BITS);
    if (oc->flash.rxdelay != 0xFF) 
        timing = (timing & ~QMI_M0_TIMING_RXDELAY_BITS) |
                 ((oc->flash.rxdelay << QMI_M0_TIMING_RXDELAY_LSB) & QMI_M0_TIMING_RXDELAY_BITS);
    
    qmidev->timing = timing;
    sleep_us(100);      // safety wait

    // do dummy read from QMI device
    *((volatile uint8_t *)XIP_NOCACHE_NOALLOC_BASE);
    __dmb();

    // set target system clock
    if (set_sys_clock_khz(oc->clk_khz, false) == false) return true;

    // configure HSTX divisor
    clock_configure_int_divider(clk_hstx, CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS, 0, clock_get_hz(clk_sys), oc->hstx_div);

    // all is ok! wait a bit and return back
    sleep_ms(200);
    return false;
}