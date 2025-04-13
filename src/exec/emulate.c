#include "rarsjs/core.h"
#include <stddef.h>

extern u32 g_regs[32];
extern u32 g_pc;
extern u32 g_mem_written_len;
extern u32 g_mem_written_addr;
extern u32 g_reg_written;
extern u32 g_error_line;

extern u32 g_runtime_error_addr;
extern Error g_runtime_error_type;

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

u8 *emulator_get_addr(u32 addr, int size) {
    u8 *memspace = NULL;
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *sec = g_sections[i];
        // TODO: check permissions
        if (addr >= sec->base && addr + size < sec->len)
            return sec->buf + (addr - sec->base);
    }
    return NULL;
}

u32 LOAD(u32 addr, int size, bool *err) {
    u8 *mem = emulator_get_addr(addr, size);
    if (!mem) {
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

    u8 *mem = emulator_get_addr(addr, size);
    if (!mem) {
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


void emulate() {
    g_runtime_error_type = ERROR_NONE;
    g_mem_written_len = 0;
    g_reg_written = 0;
    g_regs[0] = 0;
    bool err;

    u32 inst = LOAD(g_pc, 4, &err);
    if (err) {
        g_runtime_error_addr = g_pc;
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
        return;
    }

    // AUIPC
    if (opcode == 0b0010111) {
        *D = g_pc + utype;
        g_pc += 4;
        g_reg_written = rd;
        return;
    }

    // JAL
    if (opcode == 0b1101111) {
        *D = g_pc + 4;
        g_pc += jtype;
        if (rd == 1) {
            shadowstack_push();
        }
        g_reg_written = rd;
        return;
    }

    // JALR
    if (opcode == 0b1100111) {
        *D = g_pc + 4;
        g_pc = (S1 + itype) & ~1;
        // TODO: on pop, check that the addresses match
        if (rd == 1) shadowstack_push();
        if (rd == 0 && rs1 == 1) shadowstack_pop();  // jr ra/ret
        g_reg_written = rd;
        return;
    }

    // BEQ/BNE/BLT/BGE/BLTU/BGEU
    if (opcode == 0b1100011) {
        bool T = false;
        if ((funct3 >> 1) == 0) T = S1 == S2;
        else if ((funct3 >> 1) == 2) T = (i32)S1 < (i32)S2;
        else if ((funct3 >> 1) == 3) T = S1 < S2;
        else {
            g_runtime_error_addr = g_pc;
            g_runtime_error_type = ERROR_UNHANDLED_INSN;
            return;
        }
        if (funct3 & 1) T = !T;
        g_pc += T ? btype : 4;
        return;
    }

    // LB/LH/LW/LBU/LHU
    if (opcode == 0b0000011) {
        if (funct3 == 0b000) *D = sext(LOAD(S1 + itype, 1, &err), 8);
        else if (funct3 == 0b001) *D = sext(LOAD(S1 + itype, 2, &err), 16);
        else if (funct3 == 0b010) *D = LOAD(S1 + itype, 4, &err);
        else if (funct3 == 0b100) *D = LOAD(S1 + itype, 1, &err);
        else if (funct3 == 0b101) *D = LOAD(S1 + itype, 2, &err);
        else {
            g_runtime_error_addr = g_pc;
            g_runtime_error_type = ERROR_UNHANDLED_INSN;
            return;
        }
        if (err) {
            g_runtime_error_addr = S1 + itype;
            g_runtime_error_type = ERROR_LOAD;
            return;
        }
        g_pc += 4;
        g_reg_written = rd;
        return;
    }

    // SB/SH/SW
    if (opcode == 0b0100011) {
        if (funct3 == 0b000) STORE(S1 + stype, S2, 1, &err);
        else if (funct3 == 0b001) STORE(S1 + stype, S2, 2, &err);
        else if (funct3 == 0b010) STORE(S1 + stype, S2, 4, &err);
        else {
            g_runtime_error_addr = g_pc;
            g_runtime_error_type = ERROR_UNHANDLED_INSN;
            return;
        }
        if (err) {
            g_runtime_error_addr = S1 + stype;
            g_runtime_error_type = ERROR_STORE;
            return;
        }
        g_pc += 4;
        return;
    }

    // non-Load I-type
    if (opcode == 0b0010011) {
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
            g_runtime_error_addr = g_pc;
            g_runtime_error_type = ERROR_UNHANDLED_INSN;
            return;
        }
        g_pc += 4;
        g_reg_written = rd;
        return;
    }

    // R-type
    if (opcode == 0b0110011) {
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
            g_runtime_error_addr = g_pc;
            g_runtime_error_type = ERROR_UNHANDLED_INSN;
            return;
        }
        g_pc += 4;
        g_reg_written = rd;
        return;
    }

    // if i reached here, it's an unhandled instruction
    g_runtime_error_addr = g_pc;
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