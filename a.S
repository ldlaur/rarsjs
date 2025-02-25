    li x3, -10

loop:
    li x17, 1
    add x10, x0, x3
    ecall
    li x17, 11
    addi x10, x0, 10
    ecall
    addi x3, x3, 1
    bne x3, x0, loop

li x17, 93
ecall