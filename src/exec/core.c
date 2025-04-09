#include "rarsjs/core.h"


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

bool whitespace(char c) { return c == '\n' || c == '\t' || c == ' ' || c == '\r'; }
bool trailing(char c) { return c == '\t' || c == ' '; }

bool digit(char c) { return (c >= '0' && c <= '9'); }
bool ident(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') || (c == '.'); }

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
            while (p->pos < p->size && !(peek(p) == '*' && peek_n(p, 1) == '/')) advance(p);
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
                while (p->pos < p->size && !(peek(p) == '*' && peek_n(p, 1) == '/')) advance(p);
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
    char *names[] = {"zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "fp", "s1", "a0",
                     "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
                     "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
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
        *push(g_text_by_linenum, g_text_by_linenum_len, g_text_by_linenum_cap) = linenum;
    }
    asm_emit_byte(inst >> 0, linenum);
    asm_emit_byte(inst >> 8, linenum);
    asm_emit_byte(inst >> 16, linenum);
    asm_emit_byte(inst >> 24, linenum);
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

const char *label(Parser *p, Parser *orig, DeferredInsnCb *cb, const char *opcode, size_t opcode_len, u32 *out_addr,
                  bool *later) {
    *later = false;
    const char *target;
    size_t target_len;

    parse_ident(p, &target, &target_len);
    if (target_len == 0) return "No label";

    for (size_t i = 0; i < g_labels_len; i++) {
        if (g_labels[i].len == target_len && memcmp(g_labels[i].txt, target, target_len) == 0) {
            *out_addr = g_labels[i].addr;
            return NULL;
        }
    }
    if (g_in_fixup) return "Label not found";
    DeferredInsn *insn = push(g_deferred_insns, g_deferred_insn_len, g_deferred_insn_cap);
    insn->emit_idx = g_text.emit_idx;
    insn->p = *orig;
    insn->cb = cb;
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
    const char *err = label(p, &orig, handle_branch, opcode, opcode_len, &addr, &later);
    if (err) return err;
    if (later) {
        asm_emit(0, p->startline);
        return NULL;
    }
    i32 simm = addr - (g_text.emit_idx + TEXT_BASE);

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

const char *handle_branch_zero(Parser *p, const char *opcode, size_t opcode_len) {
    Parser orig = *p;
    u32 addr;
    int s;
    bool later;

    skip_whitespace(p);
    if ((s = parse_reg(p)) == -1) return "Invalid rs";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    const char *err = label(p, &orig, handle_branch_zero, opcode, opcode_len, &addr, &later);
    if (err) return err;
    if (later) {
        asm_emit(0, p->startline);
        return NULL;
    }
    i32 simm = addr - (g_text.emit_idx + TEXT_BASE);

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

const char *handle_alu_pseudo(Parser *p, const char *opcode, size_t opcode_len) {
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
    err = label(p, &orig, handle_jump, opcode, opcode_len, &addr, &later);
    if (err) return err;
    if (later) {
        asm_emit(0, p->startline);
        return NULL;
    }
    i32 simm = addr - (g_text.emit_idx + TEXT_BASE);
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
        if (simm >= -2048 && simm <= 2047) asm_emit(JALR(d, s, simm), p->startline);
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
    const char *err = label(p, &orig, handle_la, opcode, opcode_len, &addr, &later);
    if (later) {
        asm_emit(0, p->startline);
        asm_emit(0, p->startline);
        return NULL;
    }
    if (err) return err;
    i32 simm = addr - (g_text.emit_idx + TEXT_BASE);

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
        {"add", "slt", "sltu", "and", "or", "xor", "sll", "srl", "sub", "sra", "mul", "mulh", "mulu", "mulhu", "div",
         "divu", "rem", "remu"},
    },
    {handle_alu_imm, {"addi", "slt", "sltiu", "andi", "ori", "xori", "slli", "srli", "srai"}},
    {handle_ldst, {"lb", "lh", "lw", "lbu", "lhu", "sb", "sh", "sw"}},
    {handle_branch, {"beq", "bne", "blt", "bge", "bltu", "bgeu", "bgt", "ble", "bgtu", "bleu"}},
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

export void assemble(const char *txt, size_t s) {
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
                const char* ident;
                size_t ident_len;
                parse_ident(p, &ident, &ident_len);
                *push(g_globals, g_globals_len, g_globals_cap) = (Global) { .str = ident, .len = ident_len };
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
                        for (size_t i = 0; i < out_len; i++) asm_emit_byte(out[i], p->startline);
                        free(out);
                    } else break;
                    first = false;
                }
                continue;
            } else if (str_eq(directive, directive_len, "asciz") || str_eq(directive, directive_len, "string")) {
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
                        for (size_t i = 0; i < out_len; i++) asm_emit_byte(out[i], p->startline);
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
            *push(g_labels, g_labels_len, g_labels_cap) = (LabelData){.txt = ident, .len = ident_len, .addr = addr};
            continue;
        }

        opcode = ident;
        opcode_len = ident_len;

        bool found = false;
        for (size_t i = 0; !found && i < sizeof(opcode_types) / sizeof(OpcodeHandling); i++) {
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

        // see comment above skip_trailing on why this is distinct from skip_whitespace
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
            g_text.emit_idx = insn->emit_idx;
            g_section = insn->section;
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
    resolve_symbol("_start", strlen("_start"), true, &g_pc);
}

static inline i32 SIGN(int bits, u32 x) {
    int m = 32 - bits;
    return ((i32)(x << m)) >> m;
}

static inline int32_t div32(int32_t a, int32_t b) {
    if (b == 0) {
        return -1;
    } else if (a == ((int32_t)1 << (32 - 1)) && b == -1) {
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
static inline int32_t rem32(int32_t a, int32_t b) {
    if (b == 0) {
        return a;
    } else if (a == ((int32_t)1 << (32 - 1)) && b == -1) {
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

#define ERR (__builtin_unreachable(), 1)

bool check_addr_range(u32 A, u32 size) {
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *sec = g_sections[i];

        if (A >= sec->base && A < sec->limit) {
            return true;
        }
    }

    return false;
}

u32 LOAD(u32 A, int pow) {
    u8 *memspace = NULL;
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *sec = g_sections[i];

        // TODO: check read permission?
        if (A >= sec->base && A < sec->limit) {
            memspace = sec->buf;
            A -= sec->base;
            break;
        }
    }

    // TODO: flag error?
    if (NULL == memspace) {
        return 0;
    }

    if (pow == 0) return memspace[A];
    else if (pow == 1) {
        u16 tmp;
        memcpy(&tmp, memspace + A, 2);
        return tmp;
    } else if (pow == 2) {
        u32 tmp;
        memcpy(&tmp, memspace + A, 4);
        return tmp;
    }
    __builtin_unreachable();
}

void STORE(u32 A, u32 B, int pow) {
    g_mem_written_len = 1 << pow;
    g_mem_written_addr = A;

    u8 *memspace = NULL;
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *sec = g_sections[i];

        // TODO: check read permission?
        if (A >= sec->base && A < sec->limit) {
            memspace = sec->buf;
            A -= sec->base;
            break;
        }
    }

    // TODO: flag error?
    if (NULL == memspace) {
        return;
    }

    if (pow == 0) memspace[A] = B;
    else if (pow == 1) {
        memcpy(memspace + A, &B, 2);
    } else if (pow == 2) {
        memcpy(memspace + A, &B, 4);
    } else if (pow > 2) __builtin_unreachable();
    return;
}

#define BIT(n) (1UL << (n))
#define ALL_ONES(n) (BIT((n)) - 1)
#define BITS(endinclus, start) ((inst >> (start)) & ALL_ONES((endinclus) + 1 - (start)))

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
        for (int i = 0; LOAD(param + i, 0); i++) putchar(LOAD(param + i, 0));
    } else if (g_regs[17] == 11) {
        // print char
        putchar(param);
    } else if (g_regs[17] == 34) {
        // print int hex
        putchar('0');
        putchar('x');
        for (int i = 32 - 4; i >= 0; i -= 4) putchar("0123456789abcdef"[(param >> i) & 15]);
    } else if (g_regs[17] == 35) {
        // print int binary
        putchar('0');
        putchar('b');
        for (int i = 31; i >= 0; i--) {
            putchar(((param >> i) & 1) ? '1' : '0');
        }
    } else if (g_regs[17] == 93) {
        emu_exit();
    }
}


// TODO: if a label isn't found precisely
// instead of showing a raw address
// show an offset from the preceding label
// this is so ugly because i'm calling it from JS
const char* g_pc_to_label_txt;
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

// clang-format off
void emulate() {
    g_runtime_error_type = ERROR_NONE;
    g_mem_written_len = 0;
    g_regs[0] = 0;

    if (!check_addr_range(g_pc, 2)) {
        g_runtime_error_addr = g_pc;
        g_runtime_error_type = ERROR_FETCH;
        return;
    }
    u32 inst = LOAD(g_pc, 2);


    if (inst == 0x73) { do_syscall(); g_pc += 4; g_reg_written = 0; return; }
    u32 rd = BITS(11, 7);
    u32 rs1 = BITS(19, 15);
    u32 S1 = g_regs[rs1], S2 = g_regs[BITS(24, 20)], *D = &g_regs[rd];
    u32 T = 0;
    u32 F7 = BITS(31, 25), F6 = F7 >> 1;

    u32 btype_imm_ = 0, store_imm_ = 0, jal_imm_ = 0;
                            btype_imm_ |= BITS(31, 31);
    btype_imm_ <<= 1;       btype_imm_ |= BITS( 7,  7);
    btype_imm_ <<= 30-25+1; btype_imm_ |= BITS(30, 25);
    btype_imm_ <<= 11-8+1;  btype_imm_ |= BITS(11,  8);
    btype_imm_ <<= 1;       btype_imm_ |= 0;
                            store_imm_ |= BITS(31, 25);
    store_imm_ <<= 11-7+1;  store_imm_ |= BITS(11,  7);
                            jal_imm_   |= BITS(31, 31);
    jal_imm_   <<= 19-12+1; jal_imm_   |= BITS(19, 12);
    jal_imm_   <<= 20-20+1; jal_imm_   |= BITS(20, 20);
    jal_imm_   <<= 30-25+1; jal_imm_   |= BITS(30, 25);
    jal_imm_   <<= 24-21+1; jal_imm_   |= BITS(24, 21);
    jal_imm_   <<= 1;       jal_imm_   |= 0;

    i32 btype_imm = SIGN(13, btype_imm_);
    i32 store_imm = SIGN(12, store_imm_);
    i32 jal_imm   = SIGN(21, jal_imm_);
    i32 imm12_ext = (((i32)inst)>>20);
    i32 imm20_up  = BITS(31, 12) << 12;

    u32 opcode = BITS(6, 3) | (BITS(14, 12) << 4);
    u32 opcode2 = BITS(6, 2) | (BITS(14, 12) << 5);

    bool mul = F7 == 1, zero = 0, one = 0;
    if (!mul) { zero = F6 == 0, one = F6 != 0; }

    if ((opcode2 & 0b11111) == 0b01101) { *D = imm20_up; goto end; } // LUI
    if ((opcode2 & 0b11111) == 0b00101) { *D = g_pc + imm20_up; goto end; } // AUIPC
    if ((opcode2 & 0b11111) == 0b11011) { 
        *D = g_pc + 4;
        g_pc += jal_imm;
        if (rd == 1) {
            shadowstack_push();
        }
        goto exit;
    } // JAL
    if ((opcode2 & 0b11111) == 0b11001) { 
        *D = g_pc + 4;
        g_pc = S1 + imm12_ext;
        // TODO: on pop, check that the addresses match
        if (rd == 1) shadowstack_push();
        if (rd == 0 && rs1 == 1) shadowstack_pop(); // jr ra/ret
        goto exit;
    } // JALR

    if ((opcode & 0b1111) == 0b1100) {
        if ((opcode>>5) == 0) T = S1 == S2;
        if ((opcode>>5) == 1) ERR;
        if ((opcode>>5) == 2) T = (i32)S1 < (i32)S2;
        if ((opcode>>5) == 3) T = S1 < S2;
        if ((opcode>>4) & 1) T = !T;
        g_pc += T ? btype_imm : 4;
        rd = 0; 
        goto exit;
    }

    if ((opcode & 0b1111) == 0b0000) {
        T = (opcode >> 4) & 3;
        int pow = (opcode >> 4) & 3;
        if (!check_addr_range(S1 + imm12_ext, 1<<pow)) {
            g_runtime_error_addr = S1 + imm12_ext;
            g_runtime_error_type = ERROR_LOAD;
            return;
        }
        u32 load = LOAD(S1 + imm12_ext, T);
        *D = (opcode >> 6) ? load : SIGN(8 << T, load);
        goto end;
    }
    
    if ((opcode & 0b1111) == 0b0100) {
        rd = 0;
        int pow = (opcode >> 4) & 3;
        if (!check_addr_range(S1 + store_imm, 1<<pow)) {
            g_runtime_error_addr = S1 + store_imm;
            g_runtime_error_type = ERROR_STORE;
            return;
        }
        STORE(S1 + store_imm, S2, pow);
        goto end;
    }

    switch(opcode) {
    case 0b0000010: T = imm12_ext; goto inst_ADD; // ADDI
    case 0b0100010: T = imm12_ext; goto inst_SLT; // SLTI
    case 0b0110010: T = imm12_ext; goto inst_SLTU; // SLTIU
    case 0b1000010: T = imm12_ext; goto inst_XOR; // XORI
    case 0b1100010: T = imm12_ext; goto inst_OR; // ORI
    case 0b1110010: T = imm12_ext; goto inst_AND; // ANDI
    case 0b0010010: T = BITS(25, 20); goto inst_SLL; // SLLI
    case 0b1010010: T = BITS(25, 20); goto inst_SR; // SRLI/SRAI
    case 0b0000110: if (zero) { T = S2; goto inst_ADD; } *D =
                         one  ? (S1 - S2) : // SUB
                         mul  ? ((i32)S1 * (i32)S2) :
                         ERR; goto end; // MUL
    case 0b0010110: if (zero) { T = S2; goto inst_SLL; }
                     *D = ((i64)(i32)S1 * (i64)(i32)S2) >> 32; goto end; // MULH
    case 0b0100110: if (zero) { T = S2; goto inst_SLT; }
                    else *D = S1 * S2; goto end; // MULU
    case 0b0110110: if (zero) { T = S2; goto inst_SLTU; }
                    *D = ((u64)S1 * (u64)S2) >> 32; goto end; // MULHU
    case 0b1000110: if (zero) { T = S2; goto inst_XOR; }
                    *D = div32(S1,S2); goto end; // DIV
    case 0b1010110: if (!mul) { T = S2; goto inst_SR; }
                    *D = divu32(S1,S2); goto end;
    case 0b1100110: if (zero) { T = S2; goto inst_OR; } // OR
                    *D = rem32(S1, S2); goto end; // REM
    case 0b1110110: if (zero) { T = S2; goto inst_AND; } // AND
                    *D = remu32(S1, S2); goto end; // REMU
    default: ERR;
    }
    goto exit;

inst_ADD: *D = (i32)S1 + T; goto end;
inst_SLT: *D = (i32)S1 < (i32)T; goto end;
inst_SLTU: *D = S1 < T; goto end;
inst_XOR: *D = S1 ^ T; goto end;
inst_OR: *D = S1 | T; goto end;
inst_AND: *D = S1 & T; goto end;
inst_SLL: *D = S1 << (T & 0x1F); goto end;
inst_SR: *D = zero ? (S1 >> (T & 0x1F)) : ((i32)S1 >> (T & 0x1F)); goto end;

end:
    g_pc += 4;

exit:
    g_reg_written = rd;
}

bool resolve_symbol(const char *sym, size_t sym_len, bool global, u32* addr) {
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
                return true;
            }
        return false;
    }
    if (ret) {
        *addr = ret->addr;
        return true;
    }
    return false;
}

void prepare_stack() {
    g_stack = (Section){.name = "RARSJS_STACK",
                        .base = STACK_TOP - STACK_LEN,
                        .limit = STACK_TOP,
                        .len = STACK_LEN,
                        .capacity = 0,
                        .buf = NULL,
                        .emit_idx = 0,
                        .align = 1,
                        .relocations = {.buf = NULL, .len = 0, .cap = 0},
                        .read = true,
                        .write = true,
                        .execute = false,
                        .physical = false};

    g_stack.buf = malloc(g_stack.len);
    g_stack.capacity = STACK_LEN;
    g_regs[2] = STACK_TOP; // FIXME: now i am diverging from RARS, which does STACK_TOP - 4
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
    /*for (size_t i = 0; i < g_sections_len; i++) {
        free(g_sections[i]->buf);
    }*/
    free(g_sections);
    free(g_text_by_linenum);
    free(g_labels);
    free(g_deferred_insns);
    free(g_globals);
}


