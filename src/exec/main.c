#define export __attribute__((visibility("default")))
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TEXT_BASE 0x00400000
#define TEXT_END 0x10000000
#define DATA_BASE 0x10000000
#define DATA_END 0x80000000

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef uint64_t u64;
typedef int64_t i64;
typedef int32_t i32;

export u32 *g_text_by_linenum = NULL;
export size_t g_text_by_linenum_len = 0, g_text_by_linenum_cap = 0;

export u8 *g_text = NULL;
export size_t g_text_len = 0, g_text_cap = 0;
size_t g_text_emit_idx = 0;

export u8 *g_data = NULL;
export size_t g_data_len = 0, g_data_cap = 0;
size_t g_data_emit_idx = 0;

export bool g_dryrun = false;
export bool g_in_fixup = false;
export u32 g_regs[32];
export u32 g_pc = 0;
export u32 g_mem_written_len = 0;
export u32 g_mem_written_addr;
export u32 g_reg_written = 0;
export u32 g_error_line;
export const char *g_error;

typedef enum Error : u32 { ERROR_NONE = 0, ERROR_FETCH = 1, ERROR_LOAD = 2, ERROR_STORE = 3 } Error;

export u32 g_runtime_error_addr = 0;
export Error g_runtime_error_type = 0;

typedef struct Parser {
    const char *input;
    size_t pos;
    size_t size;
    int lineidx;
    int startline;
} Parser;

typedef struct LabelData {
    const char *txt;
    size_t len;
    u32 addr;
} LabelData;

typedef enum Section {
    SECTION_TEXT,
    SECTION_DATA,
} Section;
Section g_section = SECTION_TEXT;

typedef const char *DeferredInsnCb(Parser *p, const char *opcode, size_t opcode_len);
typedef struct DeferredInsn {
    Parser p;
    Section section;
    DeferredInsnCb *cb;
    const char *opcode;
    size_t opcode_len;
    size_t emit_idx;
} DeferredInsn;

LabelData *g_labels = NULL;
size_t g_labels_len = 0, g_labels_cap = 0;
DeferredInsn *g_deferred_insns = NULL;
size_t g_deferred_insn_len = 0, g_deferred_insn_cap = 0;

#ifdef __wasm__
void *malloc(size_t size);
void free(void *ptr);
extern void panic();
extern void emu_exit();
extern void putchar(uint8_t);
size_t strlen(const char *str);
int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *dest, int c, size_t n);

#define assert(cond)          \
    {                         \
        if (!(cond)) panic(); \
    }
#else

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void emu_exit() { exit(0); }
#endif

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

u32 Store(int src, int base, int off, int width) { return 0b0100011 | ((off & 31) << 7) | (width << 12) | (base << 15) | (src << 20) | ((off >> 5) << 25); }
u32 Load(int rd, int rs, int off, int width) { return 0b0000011 | (rd << 7) | (width << 12) | (rs << 15) | (off << 20); }
u32 LB(int rd, int rs, int off) { return Load(rd, rs, off, 0); }
u32 LH(int rd, int rs, int off) { return Load(rd, rs, off, 1); }
u32 LW(int rd, int rs, int off) { return Load(rd, rs, off, 2); }
u32 LBU(int rd, int rs, int off) { return Load(rd, rs, off, 4); }
u32 LHU(int rd, int rs, int off) { return Load(rd, rs, off, 5); }
u32 SB(int src, int base, int off) { return Store(src, base, off, 0); }
u32 SH(int src, int base, int off) { return Store(src, base, off, 1); }
u32 SW(int src, int base, int off) { return Store(src, base, off, 2); }
u32 Branch(int rs1, int rs2, int off, int func) { return 0b1100011 | (((off >> 11) & 1) << 7) | (((off >> 1) & 15) << 8) | (func << 12) | (rs1 << 15) | (rs2 << 20) | (((off >> 5) & 63) << 25) | (((off >> 12) & 1) << 31); }
u32 BEQ(int rs1, int rs2, int off)  { return Branch(rs1, rs2, off, 0); }
u32 BNE(int rs1, int rs2, int off)  { return Branch(rs1, rs2, off, 1); }
u32 BLT(int rs1, int rs2, int off)  { return Branch(rs1, rs2, off, 4); }
u32 BGE(int rs1, int rs2, int off)  { return Branch(rs1, rs2, off, 5); }
u32 BLTU(int rs1, int rs2, int off) { return Branch(rs1, rs2, off, 6); }
u32 BGEU(int rs1, int rs2, int off) { return Branch(rs1, rs2, off, 7); }
u32 LUI(int rd, int off) { return 0b0110111 | (rd << 7) | (off << 12); }
u32 AUIPC(int rd, int off) { return 0b0010111 | (rd << 7) | (off << 12); }
u32 JAL(int rd, int off) { return 0b1101111 | (rd << 7) | (((off >> 12) & 255) << 12) | (((off >> 11) & 1) << 20) | (((off >> 1) & 1023) << 21) | ((off >> 20) << 31); }
u32 JALR(int rd, int rs1, int off) { return 0b1100111 | (rd << 7) | (rs1 << 15) | (off << 20); }
// clang-format on

bool whitespace(char c) { return c == '\n' || c == '\t' || c == ' ' || c == '\r'; }
bool trailing(char c) { return c == '\t' || c == ' '; }

bool digit(char c) { return (c >= '0' && c <= '9'); }
bool alnum(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

#define push(arr, len, cap) \
    ((len) >= (cap) ? grow((void **)&(arr), &(cap), sizeof(*(arr))), (arr) + (len)++ : (arr) + (len)++)

static void grow(void **arr, size_t *cap, size_t size) {
    size_t oldcap = *cap;
    if (oldcap) *cap = oldcap * 2;
    else *cap = 4;
    void *newarr = malloc(*cap * size);
    memset(newarr, 0, *cap * size);
    if (arr) memcpy(newarr, *arr, oldcap * size);
    free(*arr);
    *arr = newarr;
}

void advance(Parser *p) {
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
        } else if (peek_n(p, 1) == '*') {
            advance_n(p, 2);
            while (p->pos < p->size && !(peek(p) == '*' && peek_n(p, 1) == '/')) advance(p);
            advance_n(p, 2);
        }
        return true;
    } 
    return false;
}

void skip_whitespace(Parser *p) {
    while (p->pos < p->size) {
        while (p->pos < p->size && whitespace(p->input[p->pos])) advance(p);
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
            }
            if (peek_n(p, 1) == '*') {
                advance_n(p, 2);
                while (p->pos < p->size && !(peek(p) == '*' && peek_n(p, 1) == '/')) advance(p);
                advance_n(p, 2);
                continue;
            }
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

void parse_alnum(Parser *p, const char **str, size_t *len) {
    size_t start = p->pos;
    if (p->pos < p->size && p->input[p->pos] == '-') advance(p);
    while (p->pos < p->size && alnum(p->input[p->pos])) advance(p);
    size_t end = p->pos;
    *str = p->input + start;
    *len = end - start;
}

bool str_eq(const char *txt, size_t len, const char *c) {
    if (len != strlen(c)) return false;
    return memcmp(txt, c, len) == 0;
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

int parse_reg(Parser *p) {
    const char *str;
    size_t len;
    parse_alnum(p, &str, &len);

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
    return -1;
}

void asm_emit_byte(u8 byte, int linenum) {
    if (g_dryrun) return;
    if (g_section == SECTION_TEXT) {
        if (!g_in_fixup) {
            if (g_text_emit_idx % 4 == 0) {
                *push(g_text_by_linenum, g_text_by_linenum_len, g_text_by_linenum_cap) = linenum;
            }
            *push(g_text, g_text_len, g_text_cap) = byte;
        } else g_text[g_text_emit_idx] = byte;
        g_text_emit_idx++;
    } else {
        if (!g_in_fixup) {
            *push(g_data, g_data_len, g_data_cap) = byte;
        } else g_data[g_data_emit_idx] = byte;
        g_data_emit_idx++;
    }
}

void asm_emit(u32 inst, int linenum) {
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

const char *handle_branch(Parser *p, const char *opcode, size_t opcode_len) {
    Parser orig = *p;
    const char *target;
    size_t target_len;
    int s1, s2, simm;

    skip_whitespace(p);
    if ((s1 = parse_reg(p)) == -1) return "Invalid rs1";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    if ((s2 = parse_reg(p)) == -1) return "Invalid rs2";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    parse_alnum(p, &target, &target_len);
    if (target_len == 0) return "Invalid target";

    bool found = false;
    for (size_t i = 0; i < g_labels_len; i++) {
        if (g_labels[i].len == target_len && memcmp(g_labels[i].txt, target, target_len) == 0) {
            found = true;
            simm = g_labels[i].addr - (g_text_emit_idx + TEXT_BASE);
            break;
        }
    }
    if (!found) {
        if (g_in_fixup) return "Invalid label";
        DeferredInsn *insn = push(g_deferred_insns, g_deferred_insn_len, g_deferred_insn_cap);
        insn->emit_idx = g_text_emit_idx;
        insn->p = orig;
        insn->cb = handle_branch;
        insn->opcode = opcode;
        insn->opcode_len = opcode_len;
        insn->section = g_section;
        asm_emit(0, p->startline);
        return NULL;
    }

    u32 inst = 0;
    if (str_eq(opcode, opcode_len, "beq")) inst = BEQ(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bne")) inst = BNE(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "blt")) inst = BLT(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bge")) inst = BGE(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bltu")) inst = BLTU(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bgeu")) inst = BGEU(s1, s2, simm);

    asm_emit(inst, p->startline);
    return NULL;
}

const char *handle_branch_zero(Parser *p, const char *opcode, size_t opcode_len) {
    Parser orig = *p;
    const char *target;
    size_t target_len;
    int s, simm;

    skip_whitespace(p);
    if ((s = parse_reg(p)) == -1) return "Invalid rs";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";

    skip_whitespace(p);
    parse_alnum(p, &target, &target_len);
    if (target_len == 0) return "Invalid target";

    bool found = false;
    for (size_t i = 0; i < g_labels_len; i++) {
        if (g_labels[i].len == target_len && memcmp(g_labels[i].txt, target, target_len) == 0) {
            found = true;
            simm = g_labels[i].addr - (g_text_emit_idx + TEXT_BASE);
            break;
        }
    }
    if (!found) {
        if (g_in_fixup) return "Invalid label";
        DeferredInsn *insn = push(g_deferred_insns, g_deferred_insn_len, g_deferred_insn_cap);
        insn->emit_idx = g_text_emit_idx;
        insn->p = orig;
        insn->cb = handle_branch_zero;
        insn->opcode = opcode;
        insn->opcode_len = opcode_len;
        insn->section = g_section;
        asm_emit(0, p->startline);
        return NULL;
    }

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

const char *handle_jumps(Parser *p, const char *opcode, size_t opcode_len) {
    int d, s1;
    i32 simm;

    u32 inst = 0;

    skip_whitespace(p);
    if ((d = parse_reg(p)) == -1) return "Invalid rd";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";
    skip_whitespace(p);

    if (str_eq(opcode, opcode_len, "jal")) {
        if (!parse_numeric(p, &simm)) return "Invalid imm";
        inst = JAL(d, simm);
    } else if (str_eq(opcode, opcode_len, "jalr")) {
        if ((s1 = parse_reg(p)) == -1) return "Invalid rs1";
        skip_whitespace(p);
        if (!consume_if(p, ',')) return "Expected ,";
        skip_whitespace(p);
        if (!parse_numeric(p, &simm)) return "Invalid imm";
        inst = JALR(d, s1, simm);
    }

    asm_emit(inst, p->startline);
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
        int lo = simm & 0xFFF;
        if (lo >= 0x800) lo -= 0x1000;
        int hi = (simm - lo) >> 12;
        asm_emit(LUI(d, hi), p->startline);
        asm_emit(ADDI(d, d, lo), p->startline);
    }
    return NULL;
}

const char *handle_la(Parser *p, const char *opcode, size_t opcode_len) {
    Parser orig = *p;
    int d;
    i32 simm;
    const char *target;
    size_t target_len;

    skip_whitespace(p);
    if ((d = parse_reg(p)) == -1) return "Invalid rd";
    skip_whitespace(p);
    if (!consume_if(p, ',')) return "Expected ,";
    skip_whitespace(p);

    parse_alnum(p, &target, &target_len);
    if (target_len == 0) return "Invalid target";

    bool found = false;
    for (size_t i = 0; i < g_labels_len; i++) {
        if (g_labels[i].len == target_len && memcmp(g_labels[i].txt, target, target_len) == 0) {
            found = true;
            simm = g_labels[i].addr - (g_text_emit_idx + TEXT_BASE);
            break;
        }
    }
    if (!found) {
        if (g_in_fixup) return "Invalid label";
        DeferredInsn *insn = push(g_deferred_insns, g_deferred_insn_len, g_deferred_insn_cap);
        insn->emit_idx = g_text_emit_idx;
        insn->p = orig;
        insn->cb = handle_la;
        insn->opcode = opcode;
        insn->opcode_len = opcode_len;
        insn->section = g_section;
        asm_emit(0, p->startline);
        asm_emit(0, p->startline);
        return NULL;
    }
    int lo = simm & 0xFFF;
    if (lo >= 0x800) lo -= 0x1000;
    int hi = (simm - lo) >> 12;
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
    {handle_branch, {"beq", "bne", "blt", "bge", "bltu", "bgeu"}},
    {handle_branch_zero, {"beqz", "bnez", "blez", "bgez", "bltz", "bgtz"}},
    {handle_jumps, {"jal", "jalrs"}},
    {handle_upper, {"lui", "auipc"}},
    {handle_li, {"li"}},
    {handle_la, {"la"}},
    {handle_ecall, {"ecall"}},
};

export void assemble(const char *txt, size_t s) {
    Parser parser = {0};
    parser.input = txt;
    parser.size = s;
    parser.pos = 0;
    parser.lineidx = 1;
    Parser *p = &parser;
    const char *err = NULL;

    while (1) {
        skip_whitespace(p);
        if (p->pos == p->size) break;
        p->startline = p->lineidx;
        const char *alnum, *opcode;
        size_t alnum_len, opcode_len;
        parse_alnum(p, &alnum, &alnum_len);
        skip_whitespace(p);

        if (consume_if(p, ':')) {
            u32 addr = g_section == SECTION_TEXT ? (g_text_emit_idx + TEXT_BASE) : (g_data_emit_idx + DATA_BASE);
            *push(g_labels, g_labels_len, g_labels_cap) = (LabelData){.txt = alnum, .len = alnum_len, .addr = addr};
            continue;
        } else if (consume_if(p, '.')) {
            const char *directive;
            size_t directive_len;
            parse_alnum(p, &directive, &directive_len);
            skip_whitespace(p);
            if (str_eq(directive, directive_len, "data")) {
                g_section = SECTION_DATA;
                continue;
            } else if (str_eq(directive, directive_len, "text")) {
                g_section = SECTION_TEXT;
                continue;
            } else if (str_eq(directive, directive_len, "byte")) {
                skip_whitespace(p);
                i32 value;
                if (!parse_numeric(p, &value)) {
                    err = "Invalid byte";
                    break;
                }
                if (value < -128 || value > 255) {
                    err = "Out of bounds byte";
                    break;
                }
                asm_emit_byte(value, p->startline);
                continue;
            } else if (str_eq(directive, directive_len, "half")) {
                skip_whitespace(p);
                i32 value;
                if (!parse_numeric(p, &value)) {
                    err = "Invalid half";
                    break;
                }
                if (value < -32768 || value > 65535) {
                    err = "Out of bounds half";
                    break;
                }
                // TODO: check range
                asm_emit_byte(value, p->startline);
                asm_emit_byte(value >> 8, p->startline);
                continue;
            } else if (str_eq(directive, directive_len, "word")) {
                skip_whitespace(p);
                i32 value;
                if (!parse_numeric(p, &value)) {
                    err = "Invalid word";
                    break;
                }
                asm_emit(value, p->startline);
                continue;
            }
        }

        opcode = alnum;
        opcode_len = alnum_len;

        bool found = false;
        for (int i = 0; !found && i < sizeof(opcode_types) / sizeof(OpcodeHandling); i++) {
            for (int j = 0; !found && opcode_types[i].opcodes[j]; j++) {
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
            g_text_emit_idx = insn->emit_idx;
            g_section = insn->section;
            p = &insn->p;
            err = insn->cb(&insn->p, insn->opcode, insn->opcode_len);
            if (err) break;
        }
    }

    if (err) {
        g_error = err;
        g_error_line = p->startline;
#ifndef __wasm__
        printf("line %d: %s\n", p->startline, err);
#endif
        return;
    }
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
    if (A >= TEXT_BASE && A + size <= TEXT_BASE + g_text_len) return true;
    else if (A >= DATA_BASE && A + size <= DATA_BASE + g_data_len) return true;
    return false;
}

u32 LOAD(u32 A, int pow) {
    u8 *memspace;
    if (A >= TEXT_BASE && A < TEXT_BASE + g_text_len) {
        memspace = (u8 *)g_text;
        A -= TEXT_BASE;
    } else if (A >= DATA_BASE && A < DATA_BASE + g_data_len) {
        memspace = (u8 *)g_data;
        A -= DATA_BASE;
    } else {
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
    u8 *memspace;
    if (A >= TEXT_BASE && A < TEXT_END) {
        memspace = (u8 *)g_text;
        A -= TEXT_BASE;
    } else if (A >= DATA_BASE && A < DATA_END) {
        memspace = (u8 *)g_data;
        A -= DATA_BASE;
    } else {
        return;
    }
    g_mem_written_len = 1 << pow;
    g_mem_written_addr = A;
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
    u32 S1 = g_regs[BITS(19, 15)], S2 = g_regs[BITS(24, 20)], *D = &g_regs[rd];
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
    if ((opcode2 & 0b11111) == 0b11011) { *D = g_pc + 4; g_pc += jal_imm; goto exit; } // JAL
    if ((opcode2 & 0b11111) == 0b11001) { *D = g_pc + 4; g_pc = S1 + imm12_ext; goto exit; } // JALR

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

#ifndef __wasm__

// clang-format on

void assemble_from_file(const char *src_path, bool dump) {
    FILE *f = fopen(src_path, "r");
    FILE *out = fopen("a.bin", "wb");

    fseek(f, 0, SEEK_END);
    size_t s = ftell(f);
    rewind(f);
    char *txt = malloc(s);
    fread(txt, s, 1, f);
    printf("about to assemble %zu\n", s);
    assemble(txt, s);
    if (dump) {
        fwrite(g_text, g_text_len, 1, out);
    }
}

#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s {ssemble|run|emulate} <file>\n", argv[0]);
        return -1;
    }

    if (str_eq("assemble", 8, argv[1])) {
        assemble_from_file(argv[2], true);
#ifdef _DEBUG
        printf("assembled %zu\n", (g_text_len));
#endif
    } else if (str_eq("run", 3, argv[1])) {
        fprintf(stderr, "ERROR: Not implemented yet\n");
    } else if (str_eq("emulate", 7, argv[1])) {
        assemble_from_file(argv[2], false);

        while (g_pc < TEXT_BASE + g_text_len) {
            emulate();
        }
    } else {
        fprintf(stderr, "ERROR: Unknown command: %s\n", argv[1]);
    }
}
#endif
