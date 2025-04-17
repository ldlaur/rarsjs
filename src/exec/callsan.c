#include "rarsjs/core.h"

// DO NOT MODIFY THIS STRUCT CARELESSLY
// THIS IS ACCESSED AS RAW MEMORY FROM WASM
// (this is really ugly, i know)
// possible fix: sizeof and offsetof
// and expose them to JS
// but that is quite verbose
typedef struct {
    u32 pc;       // for backtrace view
    u32 sp;       // for backtrace view
    u32 args[8];  // for backtrace view

    u32 sregs[12];
    u32 ra;
    u32 reg_bitmap;
} ShadowStackEnt;

export u32 g_reg_bitmap;
export ShadowStackEnt *g_shadow_stack;
export size_t g_shadow_stack_len, g_shadow_stack_cap;

void callsan_init() {
    // TODO: technically _start should initialize gp for main
    g_reg_bitmap =
        (1ul << REG_ZERO) | (1ul << REG_SP) | (1ul << REG_TP) | (1ul << REG_GP);
    g_shadow_stack = NULL;
    g_shadow_stack_len = 0;
    g_shadow_stack_cap = 0;
}

bool callsan_can_load(int reg) {
    if (reg == 0) return true;
    if (((g_reg_bitmap >> reg) & 1) == 0) {
        g_runtime_error_type = ERROR_CALLSAN_CANTREAD;
        g_runtime_error_addr = reg;
        return false;
    }
    return true;
}
void callsan_store(int reg) { g_reg_bitmap |= 1 << reg; }

const u32 CALLSAN_CALL_ACCESSIBLE =
    (1ul << REG_ZERO) | (1ul << REG_SP) | (1ul << REG_RA) | (1ul << REG_TP) |
    (1ul << REG_GP) | (1u << REG_A0) | (1u << REG_A1) | (1u << REG_A2) |
    (1u << REG_A3) | (1u << REG_A4) | (1u << REG_A5) | (1u << REG_A6) |
    (1u << REG_A7);

const u32 CALLSAN_CALL_CLOBBERED =
    (1u << REG_T0) | (1u << REG_T1) | (1u << REG_T2) | (1u << REG_T3) |
    (1u << REG_T4) | (1u << REG_T5) | (1u << REG_T6) | (1u << REG_A0) |
    (1u << REG_A1) | (1u << REG_A2) | (1u << REG_A3) | (1u << REG_A4) |
    (1u << REG_A5) | (1u << REG_A6) | (1u << REG_A7);

void callsan_call() {
    ShadowStackEnt *e =
        push(g_shadow_stack, g_shadow_stack_len, g_shadow_stack_cap);
    e->sregs[0] = g_regs[REG_FP];
    e->sregs[1] = g_regs[REG_S1];
    for (int i = REG_S2; i <= REG_S11; i++) e->sregs[i - REG_S2] = g_regs[i];
    for (int i = REG_A0; i <= REG_A7; i++) e->args[i - REG_A0] = g_regs[i];
    e->sp = g_regs[REG_SP];
    e->pc = g_pc;
    e->ra = g_regs[REG_RA];
    e->reg_bitmap = g_reg_bitmap;
    // only call accessible registers can be read after the call
    // &= and not = because they still must have been written to before
    g_reg_bitmap &= CALLSAN_CALL_ACCESSIBLE;
}

bool callsan_ret() {
    if (g_shadow_stack_len == 0) {
        g_runtime_error_type = ERROR_CALLSAN_RET_EMPTY;
        return false;
    }
    g_shadow_stack_len--;
    ShadowStackEnt *e = &g_shadow_stack[g_shadow_stack_len];

    if (g_regs[REG_SP] != e->sp) {
        g_runtime_error_type = ERROR_CALLSAN_SP_MISMATCH;
        return false;
    }

    if (g_regs[REG_RA] != e->ra) {
        g_runtime_error_type = ERROR_CALLSAN_RA_MISMATCH;
        return false;
    }

    u32 sregs[12];
    sregs[0] = g_regs[REG_FP];
    sregs[1] = g_regs[REG_S1];
    for (int i = 0; i < 10; i++) sregs[2 + i] = g_regs[18 + i];
    for (int i = 0; i < 12; i++) {
        if (sregs[i] != e->sregs[i]) {
            g_runtime_error_type = ERROR_CALLSAN_NOT_SAVED;
            g_runtime_error_addr = i;
            return false;
        }
    }

    // after a function return you cannot read the A and T registers
    // since the function hypothetically may have clobbered them
    g_reg_bitmap = e->reg_bitmap & ~CALLSAN_CALL_CLOBBERED;
    return true;
}