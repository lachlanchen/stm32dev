.syntax unified
.cpu cortex-m7
.fpu fpv5-d16
.thumb

.global g_pfnVectors
.global Reset_Handler

.section .isr_vector, "a", %progbits
.type g_pfnVectors, %object
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word Default_Handler
  .word Default_Handler
  .word Default_Handler
  .word Default_Handler
  .word Default_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word Default_Handler
  .word Default_Handler
  .word 0
  .word Default_Handler
  .word Default_Handler
.size g_pfnVectors, . - g_pfnVectors

.section .text.Reset_Handler, "ax", %progbits
.type Reset_Handler, %function
Reset_Handler:
  ldr r0, =_etext
  ldr r1, =_sdata
  ldr r2, =_edata
1:
  cmp r1, r2
  ittt lt
  ldrlt r3, [r0], #4
  strlt r3, [r1], #4
  blt 1b

  ldr r1, =_sbss
  ldr r2, =_ebss
  movs r3, #0
2:
  cmp r1, r2
  itt lt
  strlt r3, [r1], #4
  blt 2b

  bl main
3:
  b 3b
.size Reset_Handler, . - Reset_Handler

.section .text.Default_Handler, "ax", %progbits
.type Default_Handler, %function
Default_Handler:
  b .
.size Default_Handler, . - Default_Handler
