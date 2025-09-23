.text
    la a0, out
    lw a0, 0(a0)
    li a7, 1
    ecall
    li a7, 93
    ecall
.globl _start

.data
    out: .word -1
