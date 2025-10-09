.text
.globl _start
_start:
    la a0, array
    la a1, length
    lw a1, 0(a1)
    jal findmax
    li a7, 1
    ecall
    li a7, 93
    ecall
.data
