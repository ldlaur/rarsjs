#include "rarsjs/core.h"

#include <stddef.h>

#include "rarsjs/elf.h"

export Section g_text, g_data, g_stack;

Section **g_sections;
size_t g_sections_len, g_sections_cap;

Extern *g_externs;
size_t g_externs_len, g_externs_cap;

Section *g_section;
export u32 *g_text_by_linenum;
export size_t g_text_by_linenum_len, g_text_by_linenum_cap;

export bool g_exited;
export int g_exit_code;

export bool g_in_fixup;
export u32 g_regs[32];
export u32 g_pc;
export u32 g_mem_written_len;
export u32 g_mem_written_addr;
export u32 g_reg_written;
export u32 g_error_line;
export const char *g_error;

export u32 g_runtime_error_addr;
export Error g_runtime_error_type;

LabelData *g_labels;
size_t g_labels_len, g_labels_cap;
DeferredInsn *g_deferred_insns;
size_t g_deferred_insn_len, g_deferred_insn_cap;

extern Extern *g_externs;
extern size_t g_externs_len, g_externs_cap;

Global *g_globals;
size_t g_globals_len, g_globals_cap;

bool g_allow_externs;

// clang-format off
u32 DS1S2(u32 d, u32 s1, u32 s2) { return (d << 7) | (s1 << 15) | (s2 << 20); }
#define InstA(Name, op2, op12, one, mul) u32 Name(u32 d, u32 s1, u32 s2)  { return 0b11 | (op2 << 2) | (op12 << 12) | DS1S2(d, s1, s2) | ((one*0b01000) << 27) | (mul << 25); }
#define InstI(Name, op2, op12) u32 Name(u32 d, u32 s1, u32 imm) { return 0b11 | (op2 << 2) | ((imm & 0xfff) << 20) | (s1 << 15) | (op12 << 12) | (d << 7); }

InstI(ADDI,  0b00100, 0b000)
InstI(SLTI,  0b00100, 0b010)
InstI(SLTIU, 0b00100, 0b011)
InstI(XORI,  0b00100, 0b100)
InstI(ORI,   0b00100, 0b110)
InstI(ANDI,  0b00100, 0b111)

InstA(SLLI,  0b00100, 0b001, 0, 0)
InstA(SRLI,  0b00100, 0b101, 0, 0)
InstA(SRAI,  0b00100, 0b101, 1, 0)
InstA(ADD,   0b01100, 0b000, 0, 0)
InstA(SUB,   0b01100, 0b000, 1, 0)
InstA(MUL,   0b01100, 0b000, 0, 1)
InstA(SLL,   0b01100, 0b001, 0, 0)
InstA(MULH,  0b01100, 0b001, 0, 1)
InstA(SLT,   0b01100, 0b010, 0, 0)
InstA(MULU,  0b01100, 0b010, 0, 1)
InstA(SLTU,  0b01100, 0b011, 0, 0)
InstA(MULHU, 0b01100, 0b011, 0, 1)
InstA(XOR,   0b01100, 0b100, 0, 0)
InstA(DIV,   0b01100, 0b100, 0, 1)
InstA(SRL,   0b01100, 0b101, 0, 0)
InstA(SRA,   0b01100, 0b101, 1, 0)
InstA(DIVU,  0b01100, 0b101, 0, 1)
InstA(OR,    0b01100, 0b110, 0, 0)
InstA(REM,   0b01100, 0b110, 0, 1)
InstA(AND,   0b01100, 0b111, 0, 0)
InstA(REMU,  0b01100, 0b111, 0, 1)

u32 Store(u32 src, u32 base, u32 off, u32 width) { return 0b0100011 | ((off & 31) << 7) | (width << 12) | (base << 15) | (src << 20) | ((off >> 5) << 25); }
u32 Load(u32 rd, u32 rs, u32 off, u32 width) { return 0b0000011 | (rd << 7) | (width << 12) | (rs << 15) | (off << 20); }
u32 LB(u32 rd, u32 rs, u32 off) { return Load(rd, rs, off, 0); }
u32 LH(u32 rd, u32 rs, u32 off) { return Load(rd, rs, off, 1); }
u32 LW(u32 rd, u32 rs, u32 off) { return Load(rd, rs, off, 2); }
u32 LBU(u32 rd, u32 rs, u32 off) { return Load(rd, rs, off, 4); }
u32 LHU(u32 rd, u32 rs, u32 off) { return Load(rd, rs, off, 5); }
u32 SB(u32 src, u32 base, u32 off) { return Store(src, base, off, 0); }
u32 SH(u32 src, u32 base, u32 off) { return Store(src, base, off, 1); }
u32 SW(u32 src, u32 base, u32 off) { return Store(src, base, off, 2); }
u32 Branch(u32 rs1, u32 rs2, u32 off, u32 func) { return 0b1100011 | (((off >> 11) & 1) << 7) | (((off >> 1) & 15) << 8) | (func << 12) | (rs1 << 15) | (rs2 << 20) | (((off >> 5) & 63) << 25) | (((off >> 12) & 1) << 31); }
u32 BEQ(u32 rs1, u32 rs2, u32 off)  { return Branch(rs1, rs2, off, 0); }
u32 BNE(u32 rs1, u32 rs2, u32 off)  { return Branch(rs1, rs2, off, 1); }
u32 BLT(u32 rs1, u32 rs2, u32 off)  { return Branch(rs1, rs2, off, 4); }
u32 BGE(u32 rs1, u32 rs2, u32 off)  { return Branch(rs1, rs2, off, 5); }
u32 BLTU(u32 rs1, u32 rs2, u32 off) { return Branch(rs1, rs2, off, 6); }
u32 BGEU(u32 rs1, u32 rs2, u32 off) { return Branch(rs1, rs2, off, 7); }
u32 LUI(u32 rd, u32 off) { return 0b0110111 | (rd << 7) | (off << 12); }
u32 AUIPC(u32 rd, u32 off) { return 0b0010111 | (rd << 7) | (off << 12); }
u32 JAL(u32 rd, u32 off) { return 0b1101111 | (rd << 7) | (((off >> 12) & 255) << 12) | (((off >> 11) & 1) << 20) | (((off >> 1) & 1023) << 21) | ((off >> 20) << 31); }
u32 JALR(u32 rd, u32 rs1, u32 off) { return 0b1100111 | (rd << 7) | (rs1 << 15) | (off << 20); }
// clang-format on

bool whitespace(char c) {
    return c == '\n' || c == '\t' || c == ' ' || c == '\r';
}
bool trailing(char c) { return c == '\t' || c == ' '; }

bool digit(char c) { return (c >= '0' && c <= '9'); }
bool ident(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') || (c == '_') || (c == '.');
}

void advance(Parser *p) {
    if (p->pos >= p->size) return;
    if (p->input[p->pos] == '\n') p->lineidx++;
    p->pos++;
}

void advance_n(Parser *p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        advance(p);
    }
}

char peek(Parser *p) {
    if (p->pos >= p->size) return '\0';
    return p->input[p->pos];
}

char peek_n(Parser *p, size_t n) {
    if (p->pos + n >= p->size) return '\0';
    return p->input[p->pos + n];
}

// the difference between the whitespace and trailing functions
// is that whitespace also includes newlines
// and as such can be done between tokens in a line
// for example
//     li x0,
//        1234
// whereas i need the trailing space to end the line gracefully
// otherwise i would be marking as valid stuff like
// li x0, 1234li x0, 1234

bool skip_comment(Parser *p) {
    if (peek(p) == '/') {
        if (peek_n(p, 1) == '/') {
            while (p->pos < p->size && p->input[p->pos] != '\n') advance(p);
            return true;
        } else if (peek_n(p, 1) == '*') {
            advance_n(p, 2);
            while (p->pos < p->size && !(peek(p) == '*' && peek_n(p, 1) == '/'))
                advance(p);
            advance_n(p, 2);
            return true;
        } else {
            return false;
        }
    }
    if (peek(p) == '#') {
        while (p->pos < p->size && p->input[p->pos] != '\n') advance(p);
        return true;
    }
    return false;
}

void skip_whitespace(Parser *p) {
    while (p->pos < p->size) {
        while (whitespace(peek(p))) advance(p);
        if (!skip_comment(p)) break;
    }
}

void skip_trailing(Parser *p) {
    while (p->pos < p->size) {
        if (trailing(p->input[p->pos])) {
            advance(p);
        } else if (peek(p) == '/') {
            if (peek_n(p, 1) == '/') {
                while (p->pos < p->size && p->input[p->pos] != '\n') advance(p);
                continue;
            } else if (peek_n(p, 1) == '*') {
                advance_n(p, 2);
                while (p->pos < p->size &&
                       !(peek(p) == '*' && peek_n(p, 1) == '/'))
                    advance(p);
                advance_n(p, 2);
                continue;
            } else break;
        } else if (peek(p) == '#') {
            while (p->pos < p->size && p->input[p->pos] != '\n') advance(p);
            continue;
        } else break;
    }
}

bool consume_if(Parser *p, char c) {
    if (p->pos >= p->size) return false;
    if (p->input[p->pos] != c) return false;
    advance(p);
    return true;
}

bool consume(Parser *p, char *c) {
    if (p->pos >= p->size) return false;
    *c = p->input[p->pos];
    advance(p);
    return true;
}

void parse_ident(Parser *p, const char **str, size_t *len) {
    size_t start = p->pos;
    while (p->pos < p->size && ident(p->input[p->pos])) advance(p);
    size_t end = p->pos;
    *str = p->input + start;
    *len = end - start;
}

bool str_eq(const char *txt, size_t len, const char *c) {
    if (len != strlen(c)) return false;
    return memcmp(txt, c, len) == 0;
}

bool str_eq_2(const char *s1, size_t s1len, const char *s2, size_t s2len) {
    if (s1len != s2len) return false;
    return memcmp(s1, s2, s1len) == 0;
}

bool parse_numeric(Parser *p, i32 *out) {
    Parser start = *p;
    bool negative = false;
    bool parsed_digit = false;
    u32 value = 0;  // u32 to avoid the issue of signed overflow
    int base = 10;

    if (consume_if(p, '-')) negative = true;

    if (peek(p) == '0') {
        char prefix = peek_n(p, 1);
        if (prefix == 'x' || prefix == 'X') base = 16;
        else if (prefix == 'b' || prefix == 'B') base = 2;
        if (base != 10) advance_n(p, 2);
    }

    // TODO: handle overflow
    for (char c; (c = peek(p));) {
        int digit = base;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        if (digit >= base) break;
        parsed_digit = true;
        value = value * base + digit;
        advance(p);
    }

    if (!parsed_digit) {
        *p = start;
        return false;
    }
    if (negative) value = -value;
    *out = value;
    return true;
}

bool parse_quoted_str(Parser *p, char **out_str, size_t *out_len) {
    char *buf = NULL;
    size_t buf_len = 0, buf_cap = 0;

    bool escape = false;
    if (!consume_if(p, '"')) return false;
    while (true) {
        char c = peek(p);
        if (c == 0) {
            free(buf);
            return false;  // unquoted string
        }
        if (escape) {
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
            else if (c == 'r') c = '\r';
            else if (c == 'b') c = '\b';
            else if (c == 'f') c = '\f';
            else if (c == 'a') c = '\a';
            else if (c == 'b') c = '\b';
            else if (c == '\\') c = '\\';
            else if (c == '\'') c = '\'';
            else if (c == '"') c = '"';
            else if (c == '0') c = 0;
            else {
                free(buf);
                return false;
            }
            *push(buf, buf_len, buf_cap) = c;
            escape = false;
            advance(p);
            continue;
        }
        if (c == '\\') {
            escape = true;
            advance(p);
            continue;
        }
        if (c == '"') {
            advance(p);
            break;
        }
        *push(buf, buf_len, buf_cap) = c;
        advance(p);
    }
    *out_str = buf;
    *out_len = buf_len;
    return true;
}

int parse_reg(Parser *p) {
    const char *str;
    size_t len;
    parse_ident(p, &str, &len);

    if ((len == 2 || len == 3) && str[0] == 'x') {
        if (len == 2) return str[1] - '0';
        else {
            int num = (str[1] - '0') * 10 + (str[2] - '0');
            if (num >= 32) return -1;
            return num;
        }
    }
    char *names[] = {"zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
                     "fp",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
                     "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
                     "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
    for (int i = 0; i < 32; i++) {
        if (str_eq(str, len, names[i])) return i;
    }
    if (str_eq(str, len, "s0")) return 8;  // s0 = fp
    return -1;
}

void asm_emit_byte(u8 byte, int linenum) {
    if (!g_in_fixup) {
        *push(g_section->buf, g_section->len, g_section->capacity) = byte;
    } else {
        g_section->buf[g_section->emit_idx] = byte;
    }
    g_section->emit_idx++;
}

void asm_emit(u32 inst, int linenum) {
    if (g_section == &g_text) {
        *push(g_text_by_linenum, g_text_by_linenum_len, g_text_by_linenum_cap) =
            linenum;
    }
    asm_emit_byte(inst >> 0, linenum);
    asm_emit_byte(inst >> 8, linenum);
    asm_emit_byte(inst >> 16, linenum);
    asm_emit_byte(inst >> 24, linenum);
}

static Extern *get_extern(const char *sym, size_t sym_len) {
    for (size_t i = 0; i < g_externs_len; i++) {
        if (g_externs[i].len == sym_len &&
            0 == memcmp(sym, g_externs[i].symbol, sym_len)) {
            return &g_externs[i];
        }
    }
    Extern *e = push(g_externs, g_externs_len, g_externs_cap);
    e->symbol = sym;
    e->len = sym_len;
    return e;
}

const char *reloc_branch(const char *sym, size_t sym_len) {
    Extern *e = get_extern(sym, sym_len);
    Relocation *r = push(g_section->relocations.buf, g_section->relocations.len,
                         g_section->relocations.cap);
    r->symbol = e;
    r->addend = 0;
    r->offset = g_section->emit_idx;
    r->type = R_RISCV_BRANCH;
    return NULL;
}

const char *reloc_jal(const char *sym, size_t sym_len) {
    Extern *e = get_extern(sym, sym_len);
    Relocation *r = push(g_section->relocations.buf, g_section->relocations.len,
                         g_section->relocations.cap);
    r->symbol = e;
    r->addend = 0;
    r->offset = g_section->emit_idx;
    r->type = R_RISCV_JAL;
    return NULL;
}

const char *reloc_hi20(const char *sym, size_t sym_len) {
    Extern *e = get_extern(sym, sym_len);
    Relocation *r = push(g_section->relocations.buf, g_section->relocations.len,
                         g_section->relocations.cap);
    r->symbol = e;
    r->addend = 0;
    r->offset = g_section->emit_idx;
    r->type = R_RISCV_HI20;
    return NULL;
}

const char *reloc_lo12i(const char *sym, size_t sym_len) {
    Extern *e = get_extern(sym, sym_len);
    Relocation *r = push(g_section->relocations.buf, g_section->relocations.len,
                         g_section->relocations.cap);
    r->symbol = e;
    r->addend = 0;
    r->offset = g_section->emit_idx;
    r->type = R_RISCV_LO12_I;
    return NULL;
}

const char *reloc_lo12s(const char *sym, size_t sym_len) {
    Extern *e = get_extern(sym, sym_len);
    Relocation *r = push(g_section->relocations.buf, g_section->relocations.len,
                         g_section->relocations.cap);
    r->symbol = e;
    r->addend = 0;
    r->offset = g_section->emit_idx;
    r->type = R_RISCV_LO12_S;
    return NULL;
}

const char *reloc_hi20lo12i(const char *sym, size_t sym_len) {
    Extern *e = get_extern(sym, sym_len);
    Relocation *r = push(g_section->relocations.buf, g_section->relocations.len,
                         g_section->relocations.cap);
    r->symbol = e;
    r->addend = 0;
    r->offset = g_section->emit_idx;
    r->type = R_RISCV_HI20;
    r = push(g_section->relocations.buf, g_section->relocations.len,
             g_section->relocations.cap);
    r->symbol = e;
    r->addend = 0;
    r->offset = g_section->emit_idx + 4;
    r->type = R_RISCV_LO12_I;
    return NULL;
}

const char *reloc_hi20lo12s(const char *sym, size_t sym_len) {
    Extern *e = get_extern(sym, sym_len);
    Relocation *r = push(g_section->relocations.buf, g_section->relocations.len,
                         g_section->relocations.cap);
    r->symbol = e;
    r->addend = 0;
    r->offset = g_section->emit_idx;
    r->type = R_RISCV_HI20;
    r = push(g_section->relocations.buf, g_section->relocations.len,
             g_section->relocations.cap);
    r->symbol = e;
    r->addend = 0;
    r->offset = g_section->emit_idx + 4;
    r->type = R_RISCV_LO12_S;
    return NULL;
}

const char *reloc_abs32(const char *sym, size_t sym_len) {
    Extern *e = get_extern(sym, sym_len);
    Relocation *r = push(g_section->relocations.buf, g_section->relocations.len,
                         g_section->relocations.cap);
    r->symbol = e;
    r->addend = 0;
    r->offset = g_section->emit_idx;
    r->type = R_RISCV_32;
    return NULL;
}

const char *handle_alu_reg(Parser *p, const char *opcode, size_t opcode_len) {
    int d, s1, s2;

    skip_whitespace(p);
    if ((d = parse_reg(p)) == -1) return "Invalid rd";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    if ((s1 = parse_reg(p)) == -1) return "Invalid rs1";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    if ((s2 = parse_reg(p)) == -1) return "Invalid rs2";

    u32 inst = 0;
    if (str_eq(opcode, opcode_len, "add")) inst = ADD(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "slt")) inst = SLT(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "sltu")) inst = SLTU(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "and")) inst = AND(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "or")) inst = OR(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "xor")) inst = XOR(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "sll")) inst = SLL(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "srl")) inst = SRL(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "sub")) inst = SUB(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "sra")) inst = SRA(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "mul")) inst = MUL(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "mulh")) inst = MULH(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "mulu")) inst = MULU(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "mulhu")) inst = MULHU(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "div")) inst = DIV(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "divu")) inst = DIVU(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "rem")) inst = REM(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "remu")) inst = REMU(d, s1, s2);

    asm_emit(inst, p->startline);
    return NULL;
}

const char *handle_alu_imm(Parser *p, const char *opcode, size_t opcode_len) {
    int d, s1;
    i32 simm;

    skip_whitespace(p);
    if ((d = parse_reg(p)) == -1) return "Invalid rd";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    if ((s1 = parse_reg(p)) == -1) return "Invalid rs1";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);

    if (!parse_numeric(p, &simm)) return "Invalid imm";
    if (simm < -2048 || simm > 2047) return "Out of bounds imm";

    u32 inst = 0;
    if (str_eq(opcode, opcode_len, "addi")) inst = ADDI(d, s1, simm);
    else if (str_eq(opcode, opcode_len, "slti")) inst = SLTI(d, s1, simm);
    else if (str_eq(opcode, opcode_len, "sltiu")) inst = SLTIU(d, s1, simm);
    else if (str_eq(opcode, opcode_len, "andi")) inst = ANDI(d, s1, simm);
    else if (str_eq(opcode, opcode_len, "ori")) inst = ORI(d, s1, simm);
    else if (str_eq(opcode, opcode_len, "xori")) inst = XORI(d, s1, simm);
    else if (str_eq(opcode, opcode_len, "slli")) inst = SLLI(d, s1, simm);
    else if (str_eq(opcode, opcode_len, "srli")) inst = SRLI(d, s1, simm);
    else if (str_eq(opcode, opcode_len, "srai")) inst = SRAI(d, s1, simm);

    asm_emit(inst, p->startline);

    return NULL;
}

const char *handle_ldst(Parser *p, const char *opcode, size_t opcode_len) {
    int reg, mem;
    i32 simm;

    skip_whitespace(p);
    if ((reg = parse_reg(p)) == -1) return "Invalid rreg";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    if (!parse_numeric(p, &simm)) return "Invalid imm";

    skip_whitespace(p);

    if (!consume_if(p, '(')) return "Expected (";
    skip_whitespace(p);
    if ((mem = parse_reg(p)) == -1) return "Invalid rmem";
    skip_whitespace(p);
    if (!consume_if(p, ')')) return "Expected )";

    u32 inst = 0;
    if (str_eq(opcode, opcode_len, "lb")) inst = LB(reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "lh")) inst = LH(reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "lw")) inst = LW(reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "lbu")) inst = LBU(reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "lhu")) inst = LHU(reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "sb")) inst = SB(reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "sh")) inst = SH(reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "sw")) inst = SW(reg, mem, simm);

    asm_emit(inst, p->startline);
    return NULL;
}

const char *label(Parser *p, Parser *orig, DeferredInsnCb *cb,
                  const char *opcode, size_t opcode_len, u32 *out_addr,
                  bool *later, DeferredInsnReloc *reloc) {
    *later = false;
    const char *target;
    size_t target_len;

    parse_ident(p, &target, &target_len);
    if (target_len == 0) return "No label";

    for (size_t i = 0; i < g_labels_len; i++) {
        if (str_eq_2(g_labels[i].txt, g_labels[i].len, target, target_len)) {
            *out_addr = g_labels[i].addr;
            return NULL;
        }
    }

    if (g_in_fixup && (!reloc || !g_allow_externs)) return "Label not found";
    if (g_in_fixup) {
        *out_addr = 0;
        return reloc(target, target_len);
    }
    DeferredInsn *insn =
        push(g_deferred_insns, g_deferred_insn_len, g_deferred_insn_cap);
    insn->emit_idx = g_section->emit_idx;
    insn->p = *orig;
    insn->cb = cb;
    insn->reloc = reloc;
    insn->opcode = opcode;
    insn->opcode_len = opcode_len;
    insn->section = g_section;
    *later = true;
    return NULL;
}

const char *handle_branch(Parser *p, const char *opcode, size_t opcode_len) {
    Parser orig = *p;
    u32 addr;
    int s1, s2;
    bool later;

    skip_whitespace(p);
    if ((s1 = parse_reg(p)) == -1) return "Invalid rs1";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    if ((s2 = parse_reg(p)) == -1) return "Invalid rs2";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    const char *err = label(p, &orig, handle_branch, opcode, opcode_len, &addr,
                            &later, reloc_branch);
    if (err) return err;
    if (later) {
        asm_emit(0, p->startline);
        return NULL;
    }
    i32 simm = addr - (g_section->emit_idx + TEXT_BASE);

    u32 inst = 0;
    if (str_eq(opcode, opcode_len, "beq")) inst = BEQ(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bne")) inst = BNE(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "blt")) inst = BLT(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bge")) inst = BGE(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bltu")) inst = BLTU(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bgeu")) inst = BGEU(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bgt")) inst = BLT(s2, s1, simm);
    else if (str_eq(opcode, opcode_len, "ble")) inst = BGE(s2, s1, simm);
    else if (str_eq(opcode, opcode_len, "bgtu")) inst = BLTU(s2, s1, simm);
    else if (str_eq(opcode, opcode_len, "bleu")) inst = BGEU(s2, s1, simm);
    asm_emit(inst, p->startline);
    return NULL;
}

const char *handle_branch_zero(Parser *p, const char *opcode,
                               size_t opcode_len) {
    Parser orig = *p;
    u32 addr;
    int s;
    bool later;

    skip_whitespace(p);
    if ((s = parse_reg(p)) == -1) return "Invalid rs";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    const char *err = label(p, &orig, handle_branch_zero, opcode, opcode_len,
                            &addr, &later, reloc_branch);
    if (err) return err;
    if (later) {
        asm_emit(0, p->startline);
        return NULL;
    }
    i32 simm = addr - (g_section->emit_idx + TEXT_BASE);

    u32 inst = 0;
    if (str_eq(opcode, opcode_len, "beqz")) inst = BEQ(s, 0, simm);
    else if (str_eq(opcode, opcode_len, "bnez")) inst = BNE(s, 0, simm);
    else if (str_eq(opcode, opcode_len, "blez")) inst = BGE(0, s, simm);
    else if (str_eq(opcode, opcode_len, "bgez")) inst = BGE(s, 0, simm);
    else if (str_eq(opcode, opcode_len, "bltz")) inst = BLT(s, 0, simm);
    else if (str_eq(opcode, opcode_len, "bgtz")) inst = BLT(0, s, simm);

    asm_emit(inst, p->startline);
    return NULL;
}

const char *handle_alu_pseudo(Parser *p, const char *opcode,
                              size_t opcode_len) {
    u32 addr;
    int d, s;

    skip_whitespace(p);
    if ((d = parse_reg(p)) == -1) return "Invalid rd";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    if ((s = parse_reg(p)) == -1) return "Invalid rs";

    u32 inst = 0;
    if (str_eq(opcode, opcode_len, "mv")) inst = ADDI(d, s, 0);
    else if (str_eq(opcode, opcode_len, "not")) inst = XORI(d, s, -1);
    else if (str_eq(opcode, opcode_len, "neg")) inst = SUB(d, 0, s);
    else if (str_eq(opcode, opcode_len, "seqz")) inst = SLTIU(d, s, 1);
    else if (str_eq(opcode, opcode_len, "snez")) inst = SLTU(d, 0, s);
    else if (str_eq(opcode, opcode_len, "sltz")) inst = SLT(d, s, 0);
    else if (str_eq(opcode, opcode_len, "sgtz")) inst = SLT(d, 0, s);

    asm_emit(inst, p->startline);
    return NULL;
}

const char *handle_jump(Parser *p, const char *opcode, size_t opcode_len) {
    int d;
    Parser orig = *p;
    const char *err = NULL;
    bool later;

    skip_whitespace(p);
    // jal optionally takes a register argument
    if (str_eq(opcode, opcode_len, "jal")) {
        if ((d = parse_reg(p)) == -1) err = "Invalid rd";
        skip_whitespace(p);
        if (consume_if(p, ',')) {
            if (err) return err;
        } else {
            *p = orig;
            d = 1;
        }
    } else if (str_eq(opcode, opcode_len, "j")) {
        d = 0;
    } else assert(false);

    skip_whitespace(p);
    u32 addr;
    err = label(p, &orig, handle_jump, opcode, opcode_len, &addr, &later,
                reloc_jal);
    if (err) return err;
    if (later) {
        asm_emit(0, p->startline);
        return NULL;
    }
    i32 simm = addr - (g_section->emit_idx + TEXT_BASE);
    asm_emit(JAL(d, simm), p->startline);
    return NULL;
}

const char *handle_jump_reg(Parser *p, const char *opcode, size_t opcode_len) {
    int d, s;
    i32 simm;

    skip_whitespace(p);
    // jalr rs
    // jalr rd, rs, simm
    // jalr rd, simm(rs)
    if (str_eq(opcode, opcode_len, "jalr")) {
        if ((d = parse_reg(p)) == -1) return "Invalid register";
        skip_trailing(p);
        if (!consume_if(p, ',')) {
            asm_emit(JALR(1, d, 0), p->startline);
            return NULL;
        }
        skip_whitespace(p);
        if (parse_numeric(p, &simm)) {  // simm(rs)
            skip_whitespace(p);
            if (!consume_if(p, '(')) return "Expected (";
            skip_whitespace(p);
            if ((s = parse_reg(p)) == -1) return "Invalid rs";
            skip_whitespace(p);
            if (!consume_if(p, ')')) return "Expected )";
        } else if (consume_if(p, '(')) {  // (rs)
            simm = 0;
            skip_whitespace(p);
            if ((s = parse_reg(p)) == -1) return "Invalid rs";
            skip_whitespace(p);
            if (!consume_if(p, ')')) return "Expected )";
        } else {  // rs1, simm
            if ((s = parse_reg(p)) == -1) return "Invalid rs";
            skip_whitespace(p);
            if (!consume_if(p, ',')) return "Expected ,";
            skip_whitespace(p);
            if (!parse_numeric(p, &simm)) return "Invalid imm";
        }
        if (simm >= -2048 && simm <= 2047)
            asm_emit(JALR(d, s, simm), p->startline);
        else return "Immediate out of range";
    } else if (str_eq(opcode, opcode_len, "jr")) {
        if ((s = parse_reg(p)) == -1) return "Invalid rs";
        asm_emit(JALR(0, s, 0), p->startline);
    }
    return NULL;
}

const char *handle_ret(Parser *p, const char *opcode, size_t opcode_len) {
    asm_emit(JALR(0, 1, 0), p->startline);
    return NULL;
}

const char *handle_upper(Parser *p, const char *opcode, size_t opcode_len) {
    int d;
    i32 simm;
    u32 inst = 0;

    skip_whitespace(p);
    if ((d = parse_reg(p)) == -1) return "Invalid rd";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";
    skip_whitespace(p);
    if (!parse_numeric(p, &simm)) return "Invalid imm";

    if (str_eq(opcode, opcode_len, "lui")) inst = LUI(d, simm);
    else if (str_eq(opcode, opcode_len, "auipc")) inst = AUIPC(d, simm);

    asm_emit(inst, p->startline);
    return NULL;
}

const char *handle_li(Parser *p, const char *opcode, size_t opcode_len) {
    int d;
    i32 simm;

    skip_whitespace(p);
    if ((d = parse_reg(p)) == -1) return "Invalid rd";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";
    skip_whitespace(p);
    if (!parse_numeric(p, &simm)) return "Invalid imm";

    if (simm >= -2048 && simm <= 2047) {
        asm_emit(ADDI(d, 0, simm), p->startline);
    } else {
        u32 lo = simm & 0xFFF;
        if (lo >= 0x800) lo -= 0x1000;
        u32 hi = (u32)(simm - lo) >> 12;
        asm_emit(LUI(d, hi), p->startline);
        asm_emit(ADDI(d, d, lo), p->startline);
    }
    return NULL;
}

const char *handle_la(Parser *p, const char *opcode, size_t opcode_len) {
    Parser orig = *p;
    int d;
    bool later;

    skip_whitespace(p);
    if ((d = parse_reg(p)) == -1) return "Invalid rd";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    u32 addr;
    skip_whitespace(p);
    const char *err = label(p, &orig, handle_la, opcode, opcode_len, &addr,
                            &later, reloc_hi20lo12i);
    if (later) {
        asm_emit(0, p->startline);
        asm_emit(0, p->startline);
        return NULL;
    }
    if (err) return err;
    i32 simm = addr - (g_section->emit_idx + TEXT_BASE);

    u32 lo = simm & 0xFFF;
    if (lo >= 0x800) lo -= 0x1000;
    u32 hi = (u32)(simm - lo) >> 12;
    asm_emit(AUIPC(d, hi), p->startline);
    asm_emit(ADDI(d, d, lo), p->startline);
    return NULL;
}

const char *handle_ecall(Parser *p, const char *opcode, size_t opcode_len) {
    asm_emit(0x73, p->startline);
    return NULL;
}

typedef struct OpcodeHandling {
    DeferredInsnCb *cb;
    const char *opcodes[64];
} OpcodeHandling;

OpcodeHandling opcode_types[] = {
    {
        handle_alu_reg,
        {"add", "slt", "sltu", "and", "or", "xor", "sll", "srl", "sub", "sra",
         "mul", "mulh", "mulu", "mulhu", "div", "divu", "rem", "remu"},
    },
    {handle_alu_imm,
     {"addi", "slt", "sltiu", "andi", "ori", "xori", "slli", "srli", "srai"}},
    {handle_ldst, {"lb", "lh", "lw", "lbu", "lhu", "sb", "sh", "sw"}},
    {handle_branch,
     {"beq", "bne", "blt", "bge", "bltu", "bgeu", "bgt", "ble", "bgtu",
      "bleu"}},
    {handle_branch_zero, {"beqz", "bnez", "blez", "bgez", "bltz", "bgtz"}},
    {handle_alu_pseudo, {"mv", "not", "neg", "seqz", "snez", "sltz", "sgtz"}},
    {handle_jump, {"j", "jal"}},
    {handle_jump_reg, {"jr", "jalr"}},
    {handle_ret, {"ret"}},
    {handle_upper, {"lui", "auipc"}},
    {handle_li, {"li"}},
    {handle_la, {"la"}},
    {handle_ecall, {"ecall"}},
};

void callsan_init();

export void assemble(const char *txt, size_t s, bool allow_externs) {
    g_allow_externs = allow_externs;
    callsan_init();

    g_text = (Section){.name = ".text",
                       .base = TEXT_BASE,
                       .limit = TEXT_END,
                       .len = 0,
                       .capacity = 0,
                       .buf = NULL,
                       .emit_idx = 0,
                       .align = 4,
                       .relocations = {.buf = NULL, .len = 0, .cap = 0},
                       .read = true,
                       .write = false,
                       .execute = true,
                       .physical = true};

    g_data = (Section){.name = ".data",
                       .base = DATA_BASE,
                       .limit = DATA_END,
                       .len = 0,
                       .capacity = 0,
                       .buf = NULL,
                       .emit_idx = 0,
                       .align = 1,
                       .relocations = {.buf = NULL, .len = 0, .cap = 0},
                       .read = true,
                       .write = true,
                       .execute = false,
                       .physical = true};

    g_sections = NULL;
    g_sections_len = 0, g_sections_cap = 0;

    g_section = &g_text;
    g_text_by_linenum = NULL;
    g_text_by_linenum_len = 0, g_text_by_linenum_cap = 0;

    g_exited = false;
    g_exit_code = 0;

    g_in_fixup = false;
    memset(g_regs, 0, sizeof(g_regs));
    g_pc = TEXT_BASE;
    g_mem_written_len = 0;
    g_mem_written_addr = 0;
    g_reg_written = 0;
    g_error_line = 0;
    g_error = NULL;

    g_runtime_error_addr = 0;
    g_runtime_error_type = 0;

    g_labels = NULL;
    g_labels_len = 0, g_labels_cap = 0;
    g_deferred_insns = NULL;
    g_deferred_insn_len = 0, g_deferred_insn_cap = 0;

    g_globals = NULL;
    g_globals_len = 0, g_globals_cap = 0;

    g_externs = NULL;
    g_externs_len = 0, g_externs_cap = 0;

    prepare_runtime_sections();

    Parser parser = {0};
    parser.input = txt;
    parser.size = s;
    parser.pos = 0;
    parser.lineidx = 1;
    Parser *p = &parser;
    const char *err = NULL;

    while (1) {
        if (err) break;

        skip_whitespace(p);
        if (p->pos == p->size) break;
        p->startline = p->lineidx;

        // i can fail parsing sections
        // if so, the identifier starting with . is a temp label
        // yes, this sucks
        Parser old = *p;
        if (consume_if(p, '.')) {
            const char *directive;
            size_t directive_len;
            parse_ident(p, &directive, &directive_len);
            skip_trailing(p);
            if (str_eq(directive, directive_len, "data")) {
                g_section = &g_data;
                continue;
            } else if (str_eq(directive, directive_len, "text")) {
                g_section = &g_text;
                continue;
            } else if (str_eq(directive, directive_len, "globl")) {
                skip_trailing(p);
                const char *ident;
                size_t ident_len;
                parse_ident(p, &ident, &ident_len);
                *push(g_globals, g_globals_len, g_globals_cap) =
                    (Global){.str = ident, .len = ident_len};
                continue;
            } else if (str_eq(directive, directive_len, "byte")) {
                i32 value;
                bool first = true;
                while (true) {
                    skip_trailing(p);
                    if (first || consume_if(p, ',')) {
                        skip_trailing(p);
                        if (!parse_numeric(p, &value)) {
                            err = "Invalid byte";
                            break;
                        }
                        if (value < -128 || value > 255) {
                            err = "Out of bounds byte";
                            break;
                        }
                        asm_emit_byte(value, p->startline);
                    } else break;
                    first = false;
                }
                continue;
            } else if (str_eq(directive, directive_len, "half")) {
                i32 value;
                bool first = true;
                while (true) {
                    skip_trailing(p);
                    if (first || consume_if(p, ',')) {
                        skip_trailing(p);
                        if (!parse_numeric(p, &value)) {
                            err = "Invalid half";
                            break;
                        }
                        if (value < -32768 || value > 65535) {
                            err = "Out of bounds half";
                            break;
                        }
                        asm_emit_byte(value, p->startline);
                        asm_emit_byte(value >> 8, p->startline);
                    } else break;
                    first = false;
                }
                continue;
            } else if (str_eq(directive, directive_len, "word")) {
                i32 value;
                bool first = true;
                while (true) {
                    skip_trailing(p);
                    if (first || consume_if(p, ',')) {
                        skip_trailing(p);
                        if (!parse_numeric(p, &value)) {
                            err = "Invalid word";
                            break;
                        }
                        asm_emit(value, p->startline);
                    } else break;
                    first = false;
                }
                continue;
            } else if (str_eq(directive, directive_len, "ascii")) {
                char *out;
                size_t out_len;
                bool first = true;
                while (true) {
                    skip_trailing(p);
                    if (first || consume_if(p, ',')) {
                        skip_trailing(p);
                        if (!parse_quoted_str(p, &out, &out_len)) {
                            err = "Invalid string";
                            break;
                        }
                        for (size_t i = 0; i < out_len; i++)
                            asm_emit_byte(out[i], p->startline);
                        free(out);
                    } else break;
                    first = false;
                }
                continue;
            } else if (str_eq(directive, directive_len, "asciz") ||
                       str_eq(directive, directive_len, "string")) {
                char *out;
                size_t out_len;
                bool first = true;
                while (true) {
                    skip_trailing(p);
                    if (first || consume_if(p, ',')) {
                        skip_trailing(p);
                        if (!parse_quoted_str(p, &out, &out_len)) {
                            err = "Invalid string";
                            break;
                        }
                        for (size_t i = 0; i < out_len; i++)
                            asm_emit_byte(out[i], p->startline);
                        asm_emit_byte(0, p->startline);
                        free(out);
                    } else break;
                    first = false;
                }
                continue;
            } else {
                // backtrack if not a valid directive
                // it means that it's a label
                // so stuff like .inner_label: is valid
                *p = old;
            }
        }

        const char *ident, *opcode;
        size_t ident_len, opcode_len;
        parse_ident(p, &ident, &ident_len);
        skip_trailing(p);

        if (consume_if(p, ':')) {
            u32 addr = g_section->emit_idx + g_section->base;
            *push(g_labels, g_labels_len, g_labels_cap) =
                (LabelData){.txt = ident,
                            .len = ident_len,
                            .addr = addr,
                            .section = g_section};
            continue;
        }

        opcode = ident;
        opcode_len = ident_len;

        bool found = false;
        for (size_t i = 0;
             !found && i < sizeof(opcode_types) / sizeof(OpcodeHandling); i++) {
            for (size_t j = 0; !found && opcode_types[i].opcodes[j]; j++) {
                if (str_eq(opcode, opcode_len, opcode_types[i].opcodes[j])) {
                    found = true;
                    err = opcode_types[i].cb(p, opcode, opcode_len);
                }
            }
        }
        if (!found) {
            err = "Unknown opcode";
        }
        if (err) break;

        // see comment above skip_trailing on why this is distinct from
        // skip_whitespace
        skip_trailing(p);
        char next = peek(p);
        if (next != '\n' && next != '\0') {
            err = "Expected newline";
            break;
        }
    }

    if (!err) {
        g_in_fixup = true;
        for (size_t i = 0; i < g_deferred_insn_len; i++) {
            struct DeferredInsn *insn = &g_deferred_insns[i];
            g_section = insn->section;
            g_section->emit_idx = insn->emit_idx;
            p = &insn->p;
            err = insn->cb(&insn->p, insn->opcode, insn->opcode_len);
            if (err) break;
        }
    }

    if (err) {
        g_error = err;
        g_error_line = p->startline;
    }

    // FIXME: should i return a warning if _start is not present
    // or quietly continue?
    resolve_symbol("_start", strlen("_start"), true, &g_pc, NULL);
}


void do_syscall() {
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
            if (err) return; // TODO: return an error?
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
}

// TODO: if a label isn't found precisely
// instead of showing a raw address
// show an offset from the preceding label
// this is so ugly because i'm calling it from JS
const char *g_pc_to_label_txt;
size_t g_pc_to_label_len;
void pc_to_label(u32 pc) {
    for (size_t i = 0; i < g_labels_len; i++) {
        if (g_labels[i].addr == pc) {
            g_pc_to_label_txt = g_labels[i].txt;
            g_pc_to_label_len = g_labels[i].len;
            return;
        }
    }
    g_pc_to_label_txt = NULL;
    g_pc_to_label_len = 0;
}


bool resolve_symbol(const char *sym, size_t sym_len, bool global, u32 *addr,
                    Section **sec) {
    LabelData *ret = NULL;
    for (size_t i = 0; i < g_labels_len; i++) {
        LabelData *l = &g_labels[i];
        if (str_eq_2(sym, sym_len, l->txt, l->len)) {
            ret = l;
            break;
        }
    }
    if (ret && global) {
        for (size_t i = 0; i < g_globals_len; i++)
            if (str_eq_2(sym, sym_len, g_globals[i].str, g_globals[i].len)) {
                *addr = ret->addr;
                if (sec) {
                    *sec = ret->section;
                }
                return true;
            }
        return false;
    }
    if (ret) {
        *addr = ret->addr;
        if (sec) {
            *sec = ret->section;
        }
        return true;
    }
    return false;
}

void prepare_stack() {
    g_stack = (Section){.name = "RARSJS_STACK",
                        .base = STACK_TOP - STACK_LEN,
                        .limit = STACK_TOP,
                        .len = STACK_LEN,
                        .capacity = STACK_LEN,
                        .buf = NULL,
                        .emit_idx = 0,
                        .align = 1,
                        .relocations = {.buf = NULL, .len = 0, .cap = 0},
                        .read = true,
                        .write = true,
                        .execute = false,
                        .physical = false};

    g_stack.buf = malloc(g_stack.len);
    g_regs[2] = STACK_TOP;  // FIXME: now i am diverging from RARS, which
                            // does STACK_TOP - 4
    *push(g_sections, g_sections_len, g_sections_cap) = &g_stack;
}

void prepare_runtime_sections() {
    // TODO: dynamically growing stacks?
    // TODO: handle OOM

    *push(g_sections, g_sections_len, g_sections_cap) = &g_text;
    *push(g_sections, g_sections_len, g_sections_cap) = &g_data;
    prepare_stack();
}

void free_runtime() {
    // THIS IS COMMENTED BECAUSE SOME SECTION BUFFERS ARE NOT
    // HEAP ALLOCATED (E.G., THOSE FROM ELF FILES)
    // for (size_t i = 0; i < g_sections_len; i++) {
    //     free(g_sections[i]->relocations.buf);
    //     free(g_sections[i]->buf);
    // }
    free(g_sections);
    free(g_text_by_linenum);
    free(g_labels);
    free(g_deferred_insns);
    free(g_globals);
    free(g_externs);
}
