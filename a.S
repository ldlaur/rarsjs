.globl _start
addi x1, x1, 1234

test2:
    li a0, 1
    ret

test1:
    jal test2
    lw t0, 0(a0)
    ret

_start:
    jal test1
    li a0, 10
    li a7, 1
    ecall

    li a7, 93
    ecall
