    la t0, data1
    blez t0, boop
    blez t1, boop
    li t3, 0
forA:
    li t4, 0
forB:
    slli t2, t2, 1
    add t2, t2, t3
    add t2, t2, t4
    addi t4, t4, 1
    bne t4, t1, forB
    addi t3, t3, 1
    bne t3, t0, forA
boop:

.data
data1: .word 5