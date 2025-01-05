.syntax unified
.cpu cortex-m33
.thumb
.text

// extern "C" void bgmap_copy_8x8(uint16_t *dst, uint16_t *src, uint32_t dstfixup);
.global bgmap_copy_8x8
bgmap_copy_8x8:
    push    {r4, r5, r6, lr}
.rept       8
    ldmia   r1!, {r3-r6}
    stmia   r0!, {r3-r6}
    add     r0, r2
.endr
    pop     {r4, r5, r6, pc}

