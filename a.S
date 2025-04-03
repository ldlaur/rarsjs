.globl _start
addi x1, x1, 1234

_start:
    li a0, 10
    li a7, 1
    ecall

    li a7, 93
    ecall
