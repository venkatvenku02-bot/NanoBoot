.section .text.init
.globl _start

_start:
    la sp, boot_stack_top
    li s0, 0

    la t0, trap_entry
    csrw stvec, t0

    call kernel_main

1:
    wfi
    j 1b

.section .bss
.align 12
boot_stack:
    .space 4096
boot_stack_top:
