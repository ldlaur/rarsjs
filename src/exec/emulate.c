#include "rarsjs/emulate.h"

#include "rarsjs/callsan.h"
#include "rarsjs/core.h"
#include "rarsjs/dev.h"

export u32 g_regs[32];
export u32 g_csr[4096];
export u32 g_pc;

export u32 g_mem_written_len;
export u32 g_mem_written_addr;
export u32 g_reg_written;

export bool g_exited;
export int g_exit_code;

extern u32 g_runtime_error_params[2];
extern Error g_runtime_error_type;

static int g_privilege_level = PRIV_USER;

// end is inclusive, like in Verilog
static inline u32 extr(u32 val, u32 end, u32 start) {
    // I need to do this here because shifting by >= bitsize is UB
    if (start == 0 && end == 31) return val;
    u32 mask = (1ul << (end + 1 - start)) - 1;
    return (val >> start) & mask;
}

static inline i32 sext(u32 x, int bits) {
    int m = 32 - bits;
    return ((i32)(x << m)) >> m;
}

// Taken from Fabrice Bellard's TinyEMU
static inline i32 div32(i32 a, i32 b) {
    if (b == 0) {
        return -1;
    } else if (a == (i32)(1ul << 31) && b == -1) {
        return a;
    } else {
        return a / b;
    }
}
static inline u32 divu32(u32 a, u32 b) {
    if (b == 0) {
        return -1;
    } else {
        return a / b;
    }
}
static inline i32 rem32(i32 a, i32 b) {
    if (b == 0) {
        return a;
    } else if (a == (i32)(1ul << 31) && b == -1) {
        return 0;
    } else {
        return a % b;
    }
}
static inline u32 remu32(u32 a, u32 b) {
    if (b == 0) {
        return a;
    } else {
        return a % b;
    }
}

Section *emulator_get_section(u32 addr) {
    for (size_t i = 0; i < RARSJS_ARRAY_LEN(&g_sections); i++) {
        Section *sec = *RARSJS_ARRAY_GET(&g_sections, i);
        if (addr >= sec->base && addr <= sec->limit) {
            return sec;
        }
    }

    return NULL;
}

u8 *emulator_get_addr(u32 addr, int size, Section **out_sec) {
    Section *addr_sec = emulator_get_section(addr);

    if (out_sec) {
        *out_sec = addr_sec;
    }

    if (!addr_sec) {
        return NULL;
    }

    if (addr + size > addr_sec->contents.len + addr_sec->base) {
        return NULL;
    }

    return addr_sec->contents.buf + (addr - addr_sec->base);
    return NULL;
}

u32 LOAD(u32 addr, int size, bool *err) {
    Section *mem_sec;
    u8 *mem = emulator_get_addr(addr, size, &mem_sec);

    if (!mem_sec || !mem_sec->read ||
        (mem_sec->super && g_privilege_level == PRIV_USER)) {
        *err = true;
        return 0;
    }

    if (mem_sec->base == MMIO_BASE) {
        u32 ret;
        *err = !mmio_read(addr - MMIO_BASE, size, &ret);
        return ret;
    } else if (!mem) {
        *err = true;
        return 0;
    }

    u32 ret = 0;
    if (size == 1) {
        ret = mem[0];
    } else if (size == 2) {
        ret = mem[0];
        ret |= ((u32)mem[1]) << 8;
    } else if (size == 4) {
        ret = mem[0];
        ret |= ((u32)mem[1]) << 8;
        ret |= ((u32)mem[2]) << 16;
        ret |= ((u32)mem[3]) << 24;
    } else assert(!"Invalid size");
    *err = false;
    return ret;
}

void STORE(u32 addr, u32 val, int size, bool *err) {
    g_mem_written_len = size;
    g_mem_written_addr = addr;

    Section *mem_sec;
    u8 *mem = emulator_get_addr(addr, size, &mem_sec);

    if (!mem_sec || !mem_sec->write ||
        (mem_sec->super && g_privilege_level == PRIV_USER)) {
        *err = true;
        return;
    }

    if (mem_sec->base == MMIO_BASE) {
        *err = !mmio_write(addr - MMIO_BASE, size, val);
        return;
    } else if (!mem) {
        *err = true;
        return;
    }

    if (size == 1) {
        mem[0] = val;
    } else if (size == 2) {
        mem[0] = val;
        mem[1] = val >> 8;
    } else if (size == 4) {
        mem[0] = val;
        mem[1] = val >> 8;
        mem[2] = val >> 16;
        mem[3] = val >> 24;
    } else assert(!"Invalid size");
    *err = false;
}

void update_pending_interrupts(u32 cause) {
    if (g_csr[CSR_MIDELEG] & (1 << cause)) {
        g_csr[CSR_SIP] |= 1 << cause;
    } else {
        g_csr[CSR_MIP] |= 1 << cause;
    }
}

void jump_to_exception(u32 cause) {
    if (g_csr[CSR_MEDELEG] & (1 << cause)) {
        g_csr[CSR_SEPC] = g_pc;
        g_csr[CSR_SCAUSE] = cause;
        g_pc = g_csr[CSR_STVEC];
    } else {
        g_csr[CSR_MEPC] = g_pc;
        g_csr[CSR_MCAUSE] = cause;
        g_pc = g_csr[CSR_MTVEC];
    }

    g_pc -= 4;
}

bool jump_to_pending(u32 vec, u32 enabled, u32 *pending_ptr, u32 *cause_ptr,
                     u32 *pc_ptr) {
    if (!(*pending_ptr & enabled)) {
        return false;
    }

    *pc_ptr = g_pc;
    g_pc = vec;
    u32 cause = __builtin_ctzl(*pending_ptr & enabled);
    *pending_ptr &= ~(1 << cause);
    *cause_ptr = cause;
    return true;
}

void do_syscall() {
    u32 scause = CAUSE_U_ECALL;
    if (g_privilege_level == PRIV_SUPERVISOR) {
        scause = CAUSE_S_ECALL;
    }
    emulator_enter_kernel();

    if (!RARSJS_ARRAY_IS_EMPTY(&g_kernel_text->contents)) {
        g_csr[CSR_SEPC] = g_pc;
        g_csr[CSR_SCAUSE] = scause;
        g_csr[CSR_SEPC] = g_pc;
        g_pc = g_csr[CSR_STVEC];
        return;
    }

    g_reg_written = 0;

    u32 param = g_regs[10];
    if (g_regs[17] == 1) {
        // print int
        char buffer[12];
        int i = 0;
        if ((i32)param < 0) {
            putchar('-');
            param = -param;
        }
        do {
            buffer[i++] = (param % 10) + '0';
            param /= 10;
        } while (param > 0);
        while (i--) putchar(buffer[i]);
    } else if (g_regs[17] == 4) {
        // print string
        u32 i = 0;
        while (1) {
            bool err = false;
            u8 ch = LOAD(param + i, 1, &err);
            if (err) return;  // TODO: return an error?
            if (ch == 0) break;
            i++;
            putchar(ch);
        }
    } else if (g_regs[17] == 11) {
        // print char
        putchar(param);
    } else if (g_regs[17] == 34) {
        // print int hex
        putchar('0');
        putchar('x');
        for (int i = 32 - 4; i >= 0; i -= 4)
            putchar("0123456789abcdef"[(param >> i) & 15]);
    } else if (g_regs[17] == 35) {
        // print int binary
        putchar('0');
        putchar('b');
        for (int i = 31; i >= 0; i--) {
            putchar(((param >> i) & 1) ? '1' : '0');
        }
    } else if (g_regs[17] == 93 || g_regs[17] == 7 || g_regs[17] == 10) {
        emu_exit();
    }

    emulator_leave_kernel();
    g_pc += 4;
}

void do_sret() {
    g_pc = g_csr[CSR_SEPC] + 4;
    emulator_leave_kernel();  // TODO: ecall can come from kernel itself
}

void emulate() {
    g_runtime_error_type = ERROR_NONE;
    g_mem_written_len = 0;
    g_reg_written = 0;
    g_regs[0] = 0;
    bool err;

    // Check for interrupt
    if (!jump_to_pending(g_csr[CSR_MTVEC], g_csr[CSR_MIE], &g_csr[CSR_MIP],
                         &g_csr[CSR_MCAUSE], &g_csr[CSR_MEPC])) {
        jump_to_pending(g_csr[CSR_STVEC], g_csr[CSR_SIE], &g_csr[CSR_SIP],
                        &g_csr[CSR_SCAUSE], &g_csr[CSR_SEPC]);
    }

    u32 inst = LOAD(g_pc, 4, &err);
    if (err) {
        g_runtime_error_params[0] = g_pc;
        g_runtime_error_type = ERROR_FETCH;
        return;
    }

    u32 rd = extr(inst, 11, 7);
    u32 rs1 = extr(inst, 19, 15);
    u32 rs2 = extr(inst, 24, 20);
    u32 funct7 = extr(inst, 31, 25);
    u32 funct3 = extr(inst, 14, 12);

    i32 btype = sext((extr(inst, 31, 31) << 12) | (extr(inst, 7, 7) << 11) |
                         (extr(inst, 30, 25) << 5) | (extr(inst, 11, 8) << 1),
                     13);
    i32 stype = sext((extr(inst, 31, 25) << 5) | (extr(inst, 11, 7)), 12);
    i32 jtype = sext((extr(inst, 31, 31) << 20) | (extr(inst, 19, 12) << 12) |
                         (extr(inst, 20, 20) << 11) | (extr(inst, 30, 21) << 1),
                     21);
    i32 itype = sext(extr(inst, 31, 20), 12);
    i32 utype = extr(inst, 31, 12) << 12;

    u32 S1 = g_regs[rs1];
    u32 S2 = g_regs[rs2];
    u32 *D = &g_regs[rd];

    u32 opcode = extr(inst, 6, 0);

    // LUI
    if (opcode == 0b0110111) {
        *D = utype;
        g_pc += 4;
        g_reg_written = rd;
        callsan_store(rd);
        return;
    }

    // AUIPC
    if (opcode == 0b0010111) {
        *D = g_pc + utype;
        g_pc += 4;
        g_reg_written = rd;
        callsan_store(rd);
        return;
    }

    // JAL
    if (opcode == 0b1101111) {
        *D = g_pc + 4;
        g_pc += jtype;
        g_reg_written = rd;
        callsan_store(rd);
        if (rd == 1) callsan_call();
        return;
    }

    // JALR
    if (opcode == 0b1100111) {
        if (!callsan_can_load(rs1)) return;
        callsan_store(rd);
        *D = g_pc + 4;
        // this has to be checked before updating pc so that the highlighted pc
        // is correct
        if (rd == 0 && rs1 == 1) {  // jr ra/ret
            if (!callsan_ret()) return;
        }
        g_pc = (S1 + itype) & ~1;
        if (rd == 1) callsan_call();
        g_reg_written = rd;
        return;
    }

    // BEQ/BNE/BLT/BGE/BLTU/BGEU
    if (opcode == 0b1100011) {
        if (!callsan_can_load(rs1)) return;
        if (!callsan_can_load(rs2)) return;
        bool T = false;
        if ((funct3 >> 1) == 0) T = S1 == S2;
        else if ((funct3 >> 1) == 2) T = (i32)S1 < (i32)S2;
        else if ((funct3 >> 1) == 3) T = S1 < S2;
        else {
            g_runtime_error_params[0] = g_pc;
            g_runtime_error_type = ERROR_UNHANDLED_INSN;
            return;
        }
        if (funct3 & 1) T = !T;
        g_pc += T ? btype : 4;
        return;
    }

    // LB/LH/LW/LBU/LHU
    if (opcode == 0b0000011) {
        if (!callsan_can_load(rs1)) return;

        if (funct3 == 0b000) *D = sext(LOAD(S1 + itype, 1, &err), 8);
        else if (funct3 == 0b001) *D = sext(LOAD(S1 + itype, 2, &err), 16);
        else if (funct3 == 0b010) *D = LOAD(S1 + itype, 4, &err);
        else if (funct3 == 0b100) *D = LOAD(S1 + itype, 1, &err);
        else if (funct3 == 0b101) *D = LOAD(S1 + itype, 2, &err);
        else {
            g_runtime_error_type = ERROR_UNHANDLED_INSN;
            return;
        }
        if (err) {
            g_runtime_error_params[0] = S1 + itype;
            g_runtime_error_type = ERROR_LOAD;
            return;
        }
        if (!callsan_check_load(S1 + itype, 1 << (funct3 & 0b11))) {
            g_runtime_error_params[0] = S1 + itype;
            g_runtime_error_type = ERROR_CALLSAN_LOAD_STACK;
            return;
        }

        g_pc += 4;
        g_reg_written = rd;
        callsan_store(rd);
        return;
    }

    // SB/SH/SW
    if (opcode == 0b0100011) {
        if (!callsan_can_load(rs1)) return;
        if (!callsan_can_load(rs2)) return;
        if (funct3 == 0b000) STORE(S1 + stype, S2, 1, &err);
        else if (funct3 == 0b001) STORE(S1 + stype, S2, 2, &err);
        else if (funct3 == 0b010) STORE(S1 + stype, S2, 4, &err);
        else {
            g_runtime_error_params[0] = g_pc;
            g_runtime_error_type = ERROR_UNHANDLED_INSN;
            return;
        }
        if (err) {
            g_runtime_error_params[0] = S1 + stype;
            g_runtime_error_type = ERROR_STORE;
            return;
        }
        callsan_report_store(S1 + stype, 1 << funct3, rs2);
        g_pc += 4;
        return;
    }

    // non-Load I-type
    if (opcode == 0b0010011) {
        if (!callsan_can_load(rs1)) return;
        u32 shamt = itype & 31;
        if (funct3 == 0b000) *D = S1 + itype;                       // ADDI
        else if (funct3 == 0b010) *D = (i32)S1 < itype;             // SLTI
        else if (funct3 == 0b011) *D = S1 < (u32)itype;             // SLTIU
        else if (funct3 == 0b100) *D = S1 ^ itype;                  // XORI
        else if (funct3 == 0b110) *D = S1 | itype;                  // ORI
        else if (funct3 == 0b111) *D = S1 & itype;                  // ANDI
        else if (funct3 == 0b001 && funct7 == 0) *D = S1 << shamt;  // SLLI
        else if (funct3 == 0b101 && funct7 == 0) *D = S1 >> shamt;  // SRLI
        else if (funct3 == 0b101 && funct7 == 32)
            *D = (i32)S1 >> shamt;  // SRAI
        else {
            g_runtime_error_params[0] = g_pc;
            g_runtime_error_type = ERROR_UNHANDLED_INSN;
            return;
        }
        g_pc += 4;
        g_reg_written = rd;
        callsan_store(rd);
        return;
    }

    // R-type
    if (opcode == 0b0110011) {
        if (!callsan_can_load(rs1)) return;
        if (!callsan_can_load(rs2)) return;
        u32 shamt = S2 & 31;
        if (funct3 == 0b000 && funct7 == 0) *D = S1 + S2;                 // ADD
        else if (funct3 == 0b000 && funct7 == 32) *D = S1 - S2;           // SUB
        else if (funct3 == 0b001 && funct7 == 0) *D = S1 << shamt;        // SLL
        else if (funct3 == 0b010 && funct7 == 0) *D = (i32)S1 < (i32)S2;  // SLT
        else if (funct3 == 0b011 && funct7 == 0) *D = S1 < S2;      // SLTU
        else if (funct3 == 0b100 && funct7 == 0) *D = S1 ^ S2;      // XOR
        else if (funct3 == 0b101 && funct7 == 0) *D = S1 >> shamt;  // SRL
        else if (funct3 == 0b101 && funct7 == 32) *D = (i32)S1 >> shamt;  // SRA
        else if (funct3 == 0b110 && funct7 == 0) *D = S1 | S2;            // OR
        else if (funct3 == 0b111 && funct7 == 0) *D = S1 & S2;            // AND
        else if (funct3 == 0b000 && funct7 == 1) *D = (i32)S1 * (i32)S2;  // MUL
        else if (funct3 == 0b001 && funct7 == 1)
            *D = ((i64)(i32)S1 * (i64)(i32)S2) >> 32;  // MULH
        else if (funct3 == 0b010 && funct7 == 1)
            *D = ((i64)(i32)S1 * (i64)(u32)S2) >> 32;  // MULHSU
        else if (funct3 == 0b011 && funct7 == 1)
            *D = ((u64)S1 * (u64)S2) >> 32;                            // MULHU
        else if (funct3 == 0b100 && funct7 == 1) *D = div32(S1, S2);   // DIV
        else if (funct3 == 0b101 && funct7 == 1) *D = divu32(S1, S2);  // DIVU
        else if (funct3 == 0b110 && funct7 == 1) *D = rem32(S1, S2);   // REM
        else if (funct3 == 0b111 && funct7 == 1) *D = remu32(S1, S2);  // REMU
        else {
            g_runtime_error_params[0] = g_pc;
            g_runtime_error_type = ERROR_UNHANDLED_INSN;
            return;
        }
        g_pc += 4;
        g_reg_written = rd;
        callsan_store(rd);
        return;
    }

    // SYSTEM instructions
    if (opcode == 0x73) {
        if (funct3 == 0b000) {
            if (itype == 0x102) {  // SRET
                do_sret();
            } else {  // ECALL
                do_syscall();
            }
            return;
        } else if (funct3 == 0b001) {  // CSRRW
            u32 old = g_csr[itype];
            g_csr[itype] = rs1;
            g_regs[rd] = old;
        } else if (funct3 == 0b010) {  // CSRRS
            u32 old = g_csr[itype];
            g_csr[itype] = old | g_regs[rs1];
            g_regs[rd] = old;
        } else if (funct3 == 0b011) {  // CSRRC
            u32 old = g_csr[itype];
            g_csr[itype] = old & ~g_regs[rs1];
            g_regs[rd] = old;
        } else if (funct3 == 0b101) {  // CSRRWI
            g_regs[rd] = g_csr[itype];
            g_csr[itype] = rs1;        // used as imm
        } else if (funct3 == 0b110) {  // CSRRSI
            u32 old = g_csr[itype];
            g_csr[itype] = old | rs1;
            g_regs[rd] = old;
        } else if (funct3 == 0b111) {  // CSRRCI
            u32 old = g_csr[itype];
            g_csr[itype] = old & ~rs1;
            g_regs[rd] = old;
        } else {
            goto end;
        }

        // TODO: CSR instructions themselves are not privileged, s/m CSRs are,
        // so this is wrong, but close enough
        if (g_privilege_level == PRIV_USER) {
            g_runtime_error_params[0] = g_pc;
            g_runtime_error_type = ERROR_PROTECTION;
        }

        g_pc += 4;
        g_reg_written = rd;
        return;
    }

    // if i reached here, it's an unhandled instruction
end:
    g_runtime_error_params[0] = g_pc;
    g_runtime_error_type = ERROR_UNHANDLED_INSN;
    return;
}

// wrapper for the webui
u32 emu_load(u32 addr, int size) {
    bool err;
    u32 val = LOAD(addr, size, &err);
    if (err) return 0;
    return val;
}

void emulator_enter_kernel(void) { g_privilege_level = PRIV_SUPERVISOR; }

void emulator_leave_kernel(void) { g_privilege_level = PRIV_USER; }

void emulator_interrupt(u32 scause) {
    u32 off = scause & ~CAUSE_INTERRUPT;

    switch (scause) {
        case CAUSE_SUPERVISOR_EXTERNAL:
        case CAUSE_SUPERVISOR_SOFTWARE:
        case CAUSE_SUPERVISOR_TIMER:
            g_csr[CSR_SIP] |= (1 << off);
            break;

        case CAUSE_MACHINE_EXTERNAL:
        case CAUSE_MACHINE_SOFTWARE:
        case CAUSE_MACHINE_TIMER:
            g_csr[CSR_MIP] |= (1 << off);
            break;

        default:
            if (scause & CAUSE_INTERRUPT) {
                update_pending_interrupts(off);
            } else {
                jump_to_exception(scause);
            }
            return;
    }
}

void emulator_init(void) {
    g_exited = false;
    g_exit_code = 0;

    memset(g_regs, 0, sizeof(g_regs));
    g_pc = TEXT_BASE;
    g_mem_written_len = 0;
    g_mem_written_addr = 0;
    g_reg_written = 0;
    g_error_line = 0;
    g_error = NULL;

    memset(g_runtime_error_params, 0, sizeof(g_runtime_error_params));
    g_runtime_error_type = 0;

    prepare_aux_sections();

    g_csr[CSR_STVEC] = g_csr[CSR_SIP] = g_csr[CSR_SIE] = g_csr[CSR_SEPC] =
        g_csr[CSR_SCAUSE] = g_csr[CSR_SSTATUS] = 0;
    g_csr[CSR_MIDELEG] = g_csr[CSR_MEDELEG] = ~(u32)0;
}
