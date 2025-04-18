//
// cryptsweeper - fight the graveyard monsters and stop death
// by Pocket Pulp (@velipso), https://pulp.biz
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

    .section    .text, "x"
    .global     sys__irq_init
    .include    "reg.inc"
    .cpu        arm7tdmi
    .thumb
    .thumb_func
sys__irq_init:
    // disable interrupts during setup
    ldr   r0, =REG_IME
    movs  r1, #0
    strb  r1, [r0]

    // clear handlers
    ldr   r0, =sys__irq_vblank
    str   r1, [r0, # 0] // vblank
    str   r1, [r0, # 4] // hblank
    str   r1, [r0, # 8] // vcount
    str   r1, [r0, #12] // timer0
    str   r1, [r0, #16] // timer1
    str   r1, [r0, #20] // timer2
    str   r1, [r0, #24] // timer3
    str   r1, [r0, #28] // serial
    str   r1, [r0, #32] // dma0
    str   r1, [r0, #36] // dma1
    str   r1, [r0, #40] // dma2
    str   r1, [r0, #44] // dma3
    str   r1, [r0, #48] // keypad
    str   r1, [r0, #52] // gamepak

    // set IRQ handler
    ldr   r0, =0x03007ffc
    ldr   r1, =sys__irq_handler
    str   r1, [r0]

    // clear IE
    ldr   r0, =REG_IE
    movs  r1, #0
    strh  r1, [r0]

    // clear IF
    ldr   r0, =REG_IF
    ldr   r1, =0x3fff
    strh  r1, [r0]

    // enable interrupts
    ldr   r0, =REG_IME
    movs  r1, #1
    strb  r1, [r0]

    bx    lr

    .align 4
    .pool
    .end
