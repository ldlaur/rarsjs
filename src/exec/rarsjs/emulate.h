#pragma once

#include "core.h"

#define PRIV_MACHINE 3
#define PRIV_SUPERVISOR 1
#define PRIV_USER 0

// Interrupt bit mask (MSB)
#define CAUSE_INTERRUPT         (1 << 31)

// Exception codes (INT = 0)
#define CAUSE_INST_ADDR_MISALIGNED   0x00
#define CAUSE_INST_ACCESS_FAULT      0x01
#define CAUSE_ILLEGAL_INSTRUCTION    0x02
#define CAUSE_BREAKPOINT             0x03
#define CAUSE_LOAD_ADDR_MISALIGNED   0x04
#define CAUSE_LOAD_ACCESS_FAULT      0x05
#define CAUSE_STORE_ADDR_MISALIGNED  0x06
#define CAUSE_STORE_ACCESS_FAULT     0x07
#define CAUSE_U_ECALL                0x08
#define CAUSE_S_ECALL                0x09
#define CAUSE_VS_ECALL               0x0A
#define CAUSE_M_ECALL                0x0B
#define CAUSE_INST_PAGE_FAULT        0x0C
#define CAUSE_LOAD_PAGE_FAULT        0x0D
#define CAUSE_STORE_PAGE_FAULT       0x0F

// Interrupt codes (INT = 1)
#define CAUSE_SUPERVISOR_SOFTWARE    (CAUSE_INTERRUPT | 1)
#define CAUSE_MACHINE_SOFTWARE       (CAUSE_INTERRUPT | 3)
#define CAUSE_SUPERVISOR_TIMER       (CAUSE_INTERRUPT | 5)
#define CAUSE_MACHINE_TIMER          (CAUSE_INTERRUPT | 7)
#define CAUSE_SUPERVISOR_EXTERNAL    (CAUSE_INTERRUPT | 9)
#define CAUSE_MACHINE_EXTERNAL       (CAUSE_INTERRUPT | 11)

void emulator_enter_kernel(void);
void emulator_leave_kernel(void);
u32 LOAD(u32 addr, int size, bool *err);
void STORE(u32 addr, u32 val, int size, bool *err);
void emulator_interrupt(u32 scause);
