# CallSan
A tool to automatically check for common RISC-V calling convention violations.
## Limitations
Note that it cannot distinguish real issues from benign violations, like accessing registers or memory and not doing anything with the result.


## No restoring of caller-saved registers
```
s_overwrite:
    li s1, 1234
    ret # error will show up here

.globl _start
_start:
    jal s_overwrite
    li a7, 93
    ecall
```

```
CallSan: PC=0x400004
Callee-saved register s1 has different value at the beginning and end of the function.
Prev: 00000000
Curr: 000004d2
Check the calling convention!
```

Especially useful for SP and RA, for these common bugs
```
non_leaf:
    addi sp, sp, -4
    sw s0, 0(sp)
    jal other # ra overwritten here
    lw s0, 0(sp)
    addi sp, sp, 4
    ret # error will show up here
```

```
wrong_frame:
    addi sp, sp, -8
    # ...
    addi sp, sp, 4
    ret
```

## Using overwritten registers after function return
```
.globl _start
.text

square:
    mul a0, a0, a0 
    ret

_start:
    li t0, 0
    
    li a0, 5
    jal square
    
    addi t0, t0, 1 # error will show up here
    ecall
```
```
CallSan: PC=0x400014
Attempted to read from uninitialized register t0. Check the calling convention!
```

## Reading from stack that is not yet initialized
```
not_initialized:
    addi sp, sp, -4
    lw ra, 0(sp) # error will show up here
    sw ra, 0(sp)
    addi sp, sp, 4
    ret
```
```
CallSan: PC=0x400004
Attempted to read from stack address 0x7fffeffc, which hasn't been written to in the current function.
```
