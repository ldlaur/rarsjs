.globl _start
.globl _kernel_start

.section .kernel_text

_kernel_start:
    la a0, HelloWorld
    la a1, _CONSOLE0_OUT
loop:
    lb t0, 0(a0)
    beqz t0, exit

    sb t0, 0(a1)
    addi a0, a0, 1
    j loop
exit:

halt:
    j halt

.section .kernel_data
HelloWorld: .string "Hello, world!\n"

.text
_start:
