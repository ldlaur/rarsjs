.text
.globl _start
_start:
    la a0, length
    lw a0, 0(a0)
    la a1, array
    jal findmax
    li a7, 1
    ecall
    li a7, 93
    ecall
.data
