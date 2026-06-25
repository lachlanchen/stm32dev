.syntax unified
.cpu cortex-m7
.fpu fpv5-d16
.thumb

.equ DELAY_CONTINUE, 0x08001325

.equ LTDC_GCR,      0x50001018
.equ LTDC_L1_CFBAR, 0x500010AC
.equ FRAMEBUFFER,   0xC0000000
.equ FRAMEBUFFER_END, 0xC012C000
.equ SCB_DCIMVAC,   0xE000EF5C
.equ SCB_DCCMVAC,   0xE000EF68

.equ SAMPLE0_ADDR,  0xC008C190
.equ SAMPLE0_WORD,  0x6B2C6B4C
.equ SAMPLE1_ADDR,  0xC00BE5F0
.equ SAMPLE1_WORD,  0x316083CC

.equ IMAGE1_ADDR,   0x08006000
.equ IMAGE1_WORDS,  0x0003E800
.equ IMAGE2_ADDR,   0x08106000
.equ IMAGE2_WORDS,  0x0000C800

.section .text.persistent_trio_hook, "ax", %progbits
.global persistent_trio_hook
.type persistent_trio_hook, %function
persistent_trio_hook:
    /* Recreate the two original halfwords overwritten at 0x08001320:
       push {r4, r5, r6, lr}; mov r3, r0 */
    push {r4, r5, r6, lr}
    mov r3, r0

    push {r0, r1, r2, r3, r7, lr}
    bl persistent_trio_maybe_copy
    pop {r0, r1, r2, r3, r7, lr}

    ldr r12, =DELAY_CONTINUE
    bx r12
.size persistent_trio_hook, . - persistent_trio_hook

.type persistent_trio_maybe_copy, %function
persistent_trio_maybe_copy:
    push {r4, r5, r6, r7, lr}

    /* Only run after LTDC is enabled. */
    ldr r0, =LTDC_GCR
    ldr r1, [r0]
    tst r1, #1
    beq 9f

    /* Only run if layer 1 points at the framebuffer we identified. */
    ldr r0, =LTDC_L1_CFBAR
    ldr r1, [r0]
    ldr r2, =FRAMEBUFFER
    cmp r1, r2
    bne 9f

    bl invalidate_sample_cache_lines

    /* Cheap readback: if sample pixels already match the image, skip copy. */
    ldr r0, =SAMPLE0_ADDR
    ldr r1, [r0]
    ldr r2, =SAMPLE0_WORD
    cmp r1, r2
    bne 1f
    ldr r0, =SAMPLE1_ADDR
    ldr r1, [r0]
    ldr r2, =SAMPLE1_WORD
    cmp r1, r2
    beq 9f

1:
    ldr r0, =IMAGE1_ADDR
    ldr r1, =FRAMEBUFFER
    ldr r2, =IMAGE1_WORDS
    bl copy_words

    ldr r0, =IMAGE2_ADDR
    ldr r1, =(FRAMEBUFFER + (IMAGE1_WORDS * 4))
    ldr r2, =IMAGE2_WORDS
    bl copy_words

    bl clean_dcache_framebuffer

    dsb sy
    isb sy

9:
    pop {r4, r5, r6, r7, pc}
.size persistent_trio_maybe_copy, . - persistent_trio_maybe_copy

.type copy_words, %function
copy_words:
    cbz r2, 2f
1:
    ldr r3, [r0], #4
    str r3, [r1], #4
    subs r2, r2, #1
    bne 1b
2:
    bx lr
.size copy_words, . - copy_words

.type invalidate_sample_cache_lines, %function
invalidate_sample_cache_lines:
    ldr r2, =SCB_DCIMVAC
    ldr r0, =SAMPLE0_ADDR
    bic r0, r0, #31
    str r0, [r2]
    ldr r0, =SAMPLE1_ADDR
    bic r0, r0, #31
    str r0, [r2]
    dsb sy
    bx lr
.size invalidate_sample_cache_lines, . - invalidate_sample_cache_lines

.type clean_dcache_framebuffer, %function
clean_dcache_framebuffer:
    ldr r0, =FRAMEBUFFER
    bic r0, r0, #31
    ldr r1, =FRAMEBUFFER_END
    ldr r2, =SCB_DCCMVAC
1:
    str r0, [r2]
    adds r0, r0, #32
    cmp r0, r1
    blo 1b
    dsb sy
    bx lr
.size clean_dcache_framebuffer, . - clean_dcache_framebuffer
