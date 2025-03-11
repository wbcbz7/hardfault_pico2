.syntax unified
.cpu cortex-m33
.thumb
.text

.global fb_fill_a
fb_fill_a:
    push    {r4, r5, r6, lr}
    lsrs    r2, 4
    beq     2f
    ubfx    r1, r1, #0, #15     // clear high halfword
    orr     r1, r1, r1, lsl #16 // and duplucate with low halfword content
    mov     r4, r1
    mov     r5, r1
    mov     r6, r1
1:
    stmia   r0!, {r1, r4-r6}
    stmia   r0!, {r1, r4-r6}
    subs    r2, 1
    bne     1b
2:
    pop     {r4, r5, r6, pc}


.global fb_blend_const_a
fb_blend_const_a:
    push    {r4, r5, lr}
    lsr     r2, 4
    beq     2f
    ubfx	r1, r1, #0, #15     // clear high halfword
    orr     r1, r1, r1, lsl #16
    ldr     r3, blendmask
    and     r1, r3              // r1 = color & blendmask
1:
.set        i, 0
.rept       8
    ldr     r4, [r0, #i]
    and     r4, r3
    add     r4, r1
    lsr     r4, 1
    str     r4, [r0, #i]
.set        i, i+(1*4)
.endr
    add     r0, 1*(8*4)
    subs    r2, 1
    bne     1b
2:
    pop     {r4, r5, pc}

.global fb_blend_buf_a
fb_blend_buf_a:
    push    {r4, r5, lr}
    lsr     r2, 4
    beq     2f
    ldr     r3, blendmask
1:
.set        i, 0
.rept       8
    ldr     r4, [r0, #i]  
    ldr     r5, [r1, #i]  
    and     r4, r3        
    and     r5, r3        
    add     r4, r5        
    lsr     r4, 1         
    str     r4, [r0, #i]  
.set        i, i+(1*4)
.endr
    add     r0, 8*(1*4)
    add     r1, 8*(1*4)
    subs    r2, 1
    bne     1b
2:
    pop     {r4, r5, pc}

.align 4
blendmask: .word 0x7BDE7BDE

