.syntax unified
.cpu cortex-m7
.thumb

.section .text.branch_to_hook, "ax", %progbits
.global branch_to_hook
.type branch_to_hook, %function
branch_to_hook:
    b.w 0x08005C00
.size branch_to_hook, . - branch_to_hook
