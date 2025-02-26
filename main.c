#define export __attribute__((visibility("default")))
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef uint64_t u64;
typedef int64_t i64;
typedef int32_t i32;

#ifdef __wasm__
extern void panic();
extern void emu_exit();
extern void putchar(uint8_t);
size_t strlen(const char *str) {
    const char *s;
    for (s = str; *s; ++s);
    return s - str;
}
int memcmp(const void *s1, const void *s2, size_t n) {
    const u8 *p1 = (const u8 *)s1;
    const u8 *p2 = (const u8 *)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }
    return 0;
}
#define assert(cond) { if (!(cond)) panic(); }
#else
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
void emu_exit() { exit(0); }
#endif


#define memcpy __builtin_memcpy

u8 ram[65536];
int asm_emit_idx = 0;

u32 DS1S2(int d, int s1, int s2) { return (d << 7) | (s1 << 15) | (s2 << 20); }
#define InstA(Name, op2, op12, one, mul) u32 Name(int d, int s1, int s2)  { return 0b11 | (op2 << 2) | (op12 << 12) | DS1S2(d, s1, s2) | ((one*0b01000) << 27) | (mul << 25); }
#define InstI(Name, op2, op12) u32 Name(int d, int s1, int imm) { return 0b11 | (op2 << 2) | ((imm & 0xfff) << 20) | (s1 << 15) | (op12 << 12) | (d << 7); }

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

bool whitespace(char c) {
    return c == '\n' || c == '\t' || c == ' ';
}

bool digit(char c) {
    return (c >= '0' && c <= '9');
}

bool alnum(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

typedef struct {
    char *input;
    size_t pos;
    size_t size;
} Parser;

void skip_whitespace(Parser* p) {
    while (p->pos < p->size && whitespace(p->input[p->pos]))
        p->pos++;
}

bool parse_alnum(Parser *p, char** str, size_t* len) {
    size_t start = p->pos;
    while (p->pos < p->size && alnum(p->input[p->pos])) p->pos++;
    size_t end = p->pos;
    if (start == end) return false;
    *str = p->input + start;
    *len = end - start;
    return true;
}

bool str_eq(char* txt, size_t len, const char* c) {
    if (len != strlen(c)) return false;
    return memcmp(txt, c, len) == 0;
}

bool parse_simm(Parser *p, char** str, size_t* len) {
    size_t start = p->pos;
    if (p->pos < p->size && p->input[p->pos] == '-' ) p->pos++;
    while (p->pos < p->size && alnum(p->input[p->pos])) p->pos++;
    size_t end = p->pos;
    if (start == end) return false;
    *str = p->input + start;
    *len = end - start;
    return true;
}

int regname_to_num(char* str, size_t len) {
    if ((len == 2 || len == 3) && str[0] == 'x') {
        if (len == 2) return str[1] - '0';
        else {
            int num = (str[1]-'0')*10 + (str[2]-'0');
            if (num >= 32) return -1;
            return num;
        }
    }
    char* names[] = {"zero", "ra", "sp", "gp", "tp", "tp", "t0", "t1", "t2", "fp", "s1", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
	};
    for (int i = 0; i < 32; i++) {
        if (str_eq(str, len, names[i])) return i;
	}
    return -1;
}

int atoi_len(const char* str, int len) {
    int tmp = 0, i = 0;
    assert(len > 0);
    if (str[0] == '-') i = 1;
    for (; i < len; i++) tmp = tmp * 10 + (str[i] - '0');
    if (str[0] == '-') tmp = -tmp;
    return tmp;
}

void asm_emit(u32 inst) {
    ram[asm_emit_idx++] = inst;
    ram[asm_emit_idx++] = inst >> 8;
    ram[asm_emit_idx++] = inst >> 16;
    ram[asm_emit_idx++] = inst >> 24;
}

struct label_data {
    char* txt;
    size_t len;
    u32 addr;
};

struct label_data labels[256];
int labels_bump = 0;


typedef bool DeferredInsnCb(Parser* p, char* opcode, size_t opcode_len);
struct DeferredInsn {
    size_t emit_idx;
	DeferredInsnCb* cb;
	Parser p;
	char* opcode;
	size_t opcode_len;
};
struct DeferredInsn deferred_insns[256];
int deferred_insns_cnt = 0;


bool expect(Parser *p, char c) {
    if (p->pos >= p->size) return false;
    if (p->input[p->pos] != c) return false;
    p->pos++;
    return 1;
}

bool handle_alu_reg(Parser* p, char* opcode, size_t opcode_len) {
    char *rd, *rs1, *rs2;
    size_t rd_len, rs1_len, rs2_len;
    
    skip_whitespace(p);
    if (!parse_alnum(p, &rd, &rd_len)) return false;
    skip_whitespace(p);
    if (!expect(p, ',')) return false;

    skip_whitespace(p);
    if (!parse_alnum(p, &rs1, &rs1_len)) return false;;
    skip_whitespace(p);
    if (!expect(p, ',')) return false;

    skip_whitespace(p);
    if (!parse_alnum(p, &rs2, &rs2_len)) return false;
    skip_whitespace(p);
    
    int d, s1, s2;
    d = regname_to_num(rd, rd_len);
    assert(d != -1);
    s1 = regname_to_num(rs1, rs1_len);
    assert(s1 != -1);
    s2 = regname_to_num(rs2, rs2_len);
    assert(s2 != -1);

    u32 inst = 0;
    if      (str_eq(opcode, opcode_len, "add"))  inst = ADD  (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "slt"))  inst = SLT  (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "sltu")) inst = SLTU (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "and"))  inst = AND  (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "or"))   inst = OR   (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "xor"))  inst = XOR  (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "sll"))  inst = SLL  (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "srl"))  inst = SRL  (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "sub"))  inst = SUB  (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "sra"))  inst = SRA  (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "mul"))  inst = MUL  (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "mulh")) inst = MULH (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "mulu")) inst = MULU (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "mulhu"))inst = MULHU(d, s1, s2);
    else if (str_eq(opcode, opcode_len, "div"))  inst = DIV  (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "divu")) inst = DIVU (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "rem"))  inst = REM  (d, s1, s2);
    else if (str_eq(opcode, opcode_len, "remu")) inst = REMU (d, s1, s2);
    
    asm_emit(inst);
    return true;
}

bool handle_alu_imm(Parser* p, char* opcode, size_t opcode_len) {
    char *rd, *rs1, *imm;
    size_t rd_len, rs1_len, imm_len;
    
    skip_whitespace(p);
    if (!parse_alnum(p, &rd, &rd_len)) return false;
    skip_whitespace(p);
    if (!expect(p, ',')) return false;

    skip_whitespace(p);
    if (!parse_alnum(p, &rs1, &rs1_len)) return false;;
    skip_whitespace(p);
    if (!expect(p, ',')) return false;;

    skip_whitespace(p);
    if (!parse_simm(p, &imm, &imm_len)) return false;
    skip_whitespace(p);

    int d, s1, simm;
    d = regname_to_num(rd, rd_len);
    assert(d != -1);
    s1 = regname_to_num(rs1, rs1_len);
    assert(s1 != -1);
    simm = atoi_len(imm, imm_len);

    u32 inst = 0;
    if      (str_eq(opcode, opcode_len, "addi"))  inst = ADDI (d, s1, simm);
    else if (str_eq(opcode, opcode_len, "slti"))  inst = SLTI (d, s1, simm);
    else if (str_eq(opcode, opcode_len, "sltiu")) inst = SLTIU(d, s1, simm);
    else if (str_eq(opcode, opcode_len, "andi"))  inst = ANDI (d, s1, simm);
    else if (str_eq(opcode, opcode_len, "ori"))   inst = ORI  (d, s1, simm);
    else if (str_eq(opcode, opcode_len, "xori"))  inst = XORI (d, s1, simm);
    else if (str_eq(opcode, opcode_len, "slli"))  inst = SLLI (d, s1, simm);
    else if (str_eq(opcode, opcode_len, "srli"))  inst = SRLI (d, s1, simm);
    else if (str_eq(opcode, opcode_len, "srai"))  inst = SRAI (d, s1, simm);
    
    asm_emit(inst);

    return true;
}

bool handle_ldst(Parser* p, char* opcode, size_t opcode_len) {
    char *rreg, *rmem, *imm;
    size_t rreg_len, rmem_len, imm_len;
    
    skip_whitespace(p);
    if (!parse_alnum(p, &rreg, &rreg_len)) return false;
    skip_whitespace(p);
    if (!expect(p, ',')) return false;

    skip_whitespace(p);
    if (!parse_simm(p, &imm, &imm_len)) return false;
    skip_whitespace(p);

    if (!expect(p, '(')) return false;
    
    skip_whitespace(p);
    if (!parse_alnum(p, &rmem, &rmem_len)) return false;
    skip_whitespace(p);

    if (!expect(p, ')')) return false;

    int reg, mem, simm;
    reg = regname_to_num(rreg, rreg_len);
    assert(reg != -1);
    mem = regname_to_num(rmem, rmem_len);
    assert(mem != -1);
    simm = atoi_len(imm, imm_len);

    u32 inst = 0;
    if      (str_eq(opcode, opcode_len, "lb"))  inst = LB (reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "lh"))  inst = LH (reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "lw"))  inst = LW (reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "lbu")) inst = LBU(reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "lhu")) inst = LHU(reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "sb"))  inst = SB (reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "sh"))  inst = SH (reg, mem, simm);
    else if (str_eq(opcode, opcode_len, "sw"))  inst = SW (reg, mem, simm);

    asm_emit(inst);

    return true;
}


bool handle_branch(Parser* p, char* opcode, size_t opcode_len) {
    Parser orig = *p;

    char *rs1, *rs2, *target;
    size_t rs1_len, rs2_len, target_len;
    
    skip_whitespace(p);
    if (!parse_alnum(p, &rs1, &rs1_len)) return false;
    skip_whitespace(p);
    if (!expect(p, ',')) return false;

    skip_whitespace(p);
    if (!parse_alnum(p, &rs2, &rs2_len)) return false;
    skip_whitespace(p);
    if (!expect(p, ',')) return false;

    skip_whitespace(p);
    if (!parse_alnum(p, &target, &target_len)) return false;
    skip_whitespace(p);

    int s1, s2, simm;
    s1 = regname_to_num(rs1, rs1_len);
    assert(s1 != -1);
    s2 = regname_to_num(rs2, rs2_len);
    assert(s2 != -1);

    bool found = false;
    for (int i = 0; i < labels_bump; i++) {
        if (labels[i].len == target_len && memcmp(labels[i].txt, target, target_len) == 0) {
            found = true;
            simm = labels[i].addr - asm_emit_idx;
            break;
        }
    }
	if (!found) {
		struct DeferredInsn insn;
		insn.emit_idx = asm_emit_idx;
		insn.p = orig;
		insn.cb = handle_branch;
		insn.opcode = opcode;
		insn.opcode_len = opcode_len;
		deferred_insns[deferred_insns_cnt++] = insn;
		asm_emit_idx += 4;
		return true;
	}

    u32 inst = 0;
    if      (str_eq(opcode, opcode_len, "beq")) inst = BEQ (s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bne")) inst = BNE (s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "blt")) inst = BLT (s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bge")) inst = BGE (s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bltu"))inst = BLTU(s1, s2, simm);
    else if (str_eq(opcode, opcode_len, "bgeu"))inst = BGEU(s1, s2, simm);

    asm_emit(inst);
    return true;
}


bool handle_jumps(Parser* p, char* opcode, size_t opcode_len) {
    char *rd, *rs1, *imm;
    size_t rd_len, rs1_len, imm_len;

    skip_whitespace(p);
    if (!parse_alnum(p, &rd, &rd_len)) return false;
    skip_whitespace(p);
    if (!expect(p, ',')) return false;

    skip_whitespace(p);
    
    int d, s1, simm;
    u32 inst = 0;

    if (str_eq(opcode, opcode_len, "jal")) {
        if (!parse_alnum(p, &imm, &imm_len)) return false;
        simm = atoi_len(imm, imm_len);
        d = regname_to_num(rd, rd_len);
        assert(d != -1);
        inst = JAL(d, simm);
    } 
    else if (str_eq(opcode, opcode_len, "jalr")) {
        if (!parse_alnum(p, &rs1, &rs1_len)) return false;
        skip_whitespace(p);
        if (!expect(p, ',')) return false;
        skip_whitespace(p);

        if (!parse_alnum(p, &imm, &imm_len)) return false;
        simm = atoi_len(imm, imm_len);
        d = regname_to_num(rd, rd_len);
        s1 = regname_to_num(rs1, rs1_len);
        assert(d != -1 && s1 != -1);
        inst = JALR(d, s1, simm);
    } 

    asm_emit(inst);
    return true;
}


bool handle_upper(Parser* p, char* opcode, size_t opcode_len) {
    char *rd, *rs1, *imm;
    size_t rd_len, rs1_len, imm_len;

    skip_whitespace(p);
    if (!parse_alnum(p, &rd, &rd_len)) return false;
    skip_whitespace(p);
    if (!expect(p, ',')) return false;

    skip_whitespace(p);
    
    int d, s1, simm;
    u32 inst = 0;

    if (!parse_simm(p, &imm, &imm_len)) return false;
    simm = atoi_len(imm, imm_len);
    d = regname_to_num(rd, rd_len);
    assert(d != -1);

    if (str_eq(opcode, opcode_len, "lui")) inst = LUI(d, simm);
    else if (str_eq(opcode, opcode_len, "auipc")) inst = AUIPC(d, simm);

    asm_emit(inst);

    return true;
}


bool handle_li(Parser* p, char* opcode, size_t opcode_len) {
    char *rd, *imm;
    size_t rd_len, imm_len;

    skip_whitespace(p);
    if (!parse_alnum(p, &rd, &rd_len)) return false;
    skip_whitespace(p);
    if (!expect(p, ',')) return false;

    skip_whitespace(p);

    int d, simm;
    u32 inst = 0;

    if (!parse_simm(p, &imm, &imm_len)) return false;
    simm = atoi_len(imm, imm_len);
    d = regname_to_num(rd, rd_len);
    assert(d != -1);

    if (simm >= -2048 && simm <= 2047) {
        asm_emit(ADDI(d, 0, simm));
    } else {
        int lo = simm & 0xFFF;
        if (lo >= 0x800) lo -= 0x1000;
        int hi = (simm - lo) >> 12;
        asm_emit(LUI(d, hi));
        asm_emit(ADDI(d, d, lo)); 
    }
    return true;
}


void handle_ecall() {
    asm_emit(0x73);
}


export void assemble(char* txt, size_t s) {
    // split in lines
    size_t ti = 0;

    char* opcodes_alu_reg[] = {
        "add", "slt", "sltu", "and", "or", "xor", "sll", "srl", "sub", "sra",
        "mul", "mulh", "mulu", "mulhu", "div", "divu", "rem", "remu"
    };
    char* opcodes_alu_imm[] = {
        "addi", "slt", "sltiu", "andi", "ori", "xori",
        "slli", "srli", "srai"
    };
    char* opcodes_ldst[] = {
        "lb", "lh", "lw", "lbu", "lhu", "sb", "sh", "sw"
    };
    char* opcodes_branch[] = {
        "beq", "bne", "blt", "bge", "bltu", "bgeu"
    };
    Parser parser;
    parser.input = txt;
    parser.size = s;
    parser.pos = 0;
    Parser* p = &parser;

    while (1) {
        skip_whitespace(p);
        if (p->pos == p->size) break;
        char *alnum, *opcode;
        size_t alnum_len, opcode_len;
        if (!parse_alnum(p, &alnum, &alnum_len)) break;
        skip_whitespace(p);
        if (p->pos == p->size) break;

        if (expect(p, ':')) {
            labels[labels_bump++] = (struct label_data){ .txt = alnum, .len = alnum_len, .addr = asm_emit_idx };
            continue;
        } else {
            opcode = alnum;
            opcode_len = alnum_len;
        }

        bool found = false;
        for (int i = 0; !found && i < sizeof(opcodes_alu_imm)/sizeof(char*); i++) {
            if (str_eq(opcode, opcode_len, opcodes_alu_imm[i])) {
                if (!handle_alu_imm(p, opcode, opcode_len)) break;
                found = true;
            }
        }
        for (int i = 0; !found && i < sizeof(opcodes_alu_reg)/sizeof(char*); i++) {
            if (str_eq(opcode, opcode_len, opcodes_alu_reg[i])) {
                if (!handle_alu_reg(p, opcode, opcode_len)) break;
                found = true;
            }
        }
        
        for (int i = 0; !found && i < sizeof(opcodes_ldst)/sizeof(char*); i++) {
            if (str_eq(opcode, opcode_len, opcodes_ldst[i])) {
                if (!handle_ldst(p, opcode, opcode_len)) break;
                found = true;
            }
        }

        for (int i = 0; !found && i < sizeof(opcodes_branch)/sizeof(char*); i++) {
            if (str_eq(opcode, opcode_len, opcodes_branch[i])) {
                if (!handle_branch(p, opcode, opcode_len)) break;
                found = true;
            }
        }

        if (str_eq(opcode, opcode_len, "jal") || str_eq(opcode, opcode_len, "jalr")) {
            if (!handle_jumps(p, opcode, opcode_len)) break;
            found = true;
        }

        if (str_eq(opcode, opcode_len, "lui") || str_eq(opcode, opcode_len, "auipc")) {
            if (!handle_upper(p, opcode, opcode_len)) break;
            found = true;
        }

        if (str_eq(opcode, opcode_len, "li")) {
            if (!handle_li(p, opcode, opcode_len)) break;
            found = true;
        }

        if (str_eq(opcode, opcode_len, "ecall")) {
            handle_ecall();
            found = true;
        }

        assert(found);
    }
	printf("deferring\n");
    int oldemit = asm_emit_idx;
	for (int i = 0; i < deferred_insns_cnt; i++) {
		struct DeferredInsn* insn = &deferred_insns[i];
		asm_emit_idx = insn->emit_idx;
		insn->cb(&insn->p, insn->opcode, insn->opcode_len);
		assert(asm_emit_idx == insn->emit_idx + 4);
	}
	asm_emit_idx = oldemit;
}

uint32_t regs[32];
uint32_t ip = 0;

static inline i32 SIGN(int bits, u32 x) {
    int m = 32 - bits;
    return ((i32)(x << m)) >> m;
}

static inline int32_t div32(int32_t a, int32_t b) {
    if (b == 0) { return -1; }
    else if (a == ((int32_t)1 << (32 - 1)) && b == -1) { return a; }
    else { return a / b; }
}
static inline uint32_t divu32(uint32_t a, uint32_t b) {
    if (b == 0) { return -1; }
    else { return a / b; }
}
static inline int32_t rem32(int32_t a, int32_t b) {
    if (b == 0) { return a; }
    else if (a == ((int32_t)1 << (32 - 1)) && b == -1) { return 0; }
    else { return a % b; }
}
static inline uint32_t remu32(uint32_t a, uint32_t b) {
    if (b == 0) { return a; }
    else { return a % b; }
}

#define ERR (__builtin_unreachable(), 1)

u32 LOAD(u32 A, int pow) {
    if (pow == 0) return ram[A];
    else if (pow == 1) { u16 tmp; memcpy(&tmp, ram+A, 2); return tmp; }
    else if (pow == 2) { u32 tmp; memcpy(&tmp, ram+A, 4); return tmp; }
    __builtin_unreachable();
}

void STORE(u32 A, u32 B, int pow) {
    if (pow == 0) ram[A] = B;
    else if (pow == 1) { memcpy(ram+A, &B, 2); }
    else if (pow == 2) { memcpy(ram+A, &B, 4); }
    else if (pow > 2) __builtin_unreachable();
    return;
}


#define BIT(n) (1UL << (n))
#define ALL_ONES(n) (BIT((n)) - 1)
#define BITS(endinclus, start) ((inst >> (start)) & ALL_ONES((endinclus) + 1 - (start)))

void do_syscall() {
    u32 param = regs[10];
    if (regs[17] == 1) {
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
        while (i--)
            putchar(buffer[i]);
    } else if (regs[17] == 4) {
        // print string
        for (int i = 0; ram[param+i]; i++) putchar(ram[param+i]);
    } else if (regs[17] == 11) {
        // print char
        putchar(param);
    } else if (regs[17] == 34) {
        // print int hex
        putchar('0');
        putchar('x');
        for (int i = 32-4; i >= 0; i -= 4)
            putchar("0123456789abcdef"[(param >> i) & 15]);
    } else if (regs[17] == 35) {
        // print int binary
        putchar('0');
        putchar('b');
        for (int i = 31; i >= 0; i--) {
            putchar(((param >> i) & 1) ? '1' : '0');
        }
    } else if (regs[17] == 93) {
        emu_exit();
    }
}

void emulate() {
    regs[0] = 0;
    uint32_t inst = LOAD(ip, 2);
    uint32_t A = ip;


    if (inst == 0x73) { do_syscall(); ip += 4; return; }
    u32 S1 = regs[BITS(19, 15)], S2 = regs[BITS(24, 20)], *D = &regs[BITS(11, 7)];
    u32 T = 0;
    u32 F7 = BITS(31, 25), F6 = F7 >> 1, F3 = BITS(14,12);

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
    u32 funct3;

    if ((opcode2 & 0b11111) == 0b01101) { *D = imm20_up; goto end; } // LUI
    if ((opcode2 & 0b11111) == 0b00101) { *D = ip + imm20_up; goto end; } // AUIPC
    if ((opcode2 & 0b11111) == 0b11011) { *D = ip + 4; ip += jal_imm; return; } // JAL
    if ((opcode2 & 0b11111) == 0b11001) { *D = ip + 4; ip = S1 + imm12_ext; return; } // JALR

    if ((opcode & 0b1111) == 0b1100) {
        if ((opcode>>5) == 0) T = S1 == S2;
        if ((opcode>>5) == 1) ERR;
        if ((opcode>>5) == 2) T = (i32)S1 < (i32)S2;
        if ((opcode>>5) == 3) T = S1 < S2;
        if ((opcode>>4) & 1) T = !T;
        ip += T ? btype_imm : 4;
        return;
    }

    if ((opcode & 0b1111) == 0b0000) {
        T = (opcode >> 4) & 3;
        u32 load = LOAD(S1 + imm12_ext, T);
        *D = (opcode >> 6) ? load : SIGN(8 << T, load);
        goto end;
    }
    
    if ((opcode & 0b1111) == 0b0100) {
        STORE(S1 + store_imm, S2, (opcode >> 4) & 3); goto end;
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
    return;

inst_ADD: *D = (i32)S1 + T; goto end;
inst_SLT: *D = (i32)S1 < (i32)T; goto end;
inst_SLTU: *D = S1 < T; goto end;
inst_XOR: *D = S1 ^ T; goto end;
inst_OR: *D = S1 | T; goto end;
inst_AND: *D = S1 & T; goto end;
inst_SLL: *D = S1 << (T & 0x1F); goto end;
inst_SR: *D = zero ? (S1 >> (T & 0x1F)) : ((i32)S1 >> (T & 0x1F)); goto end;

end:
    ip += 4;
}


#ifndef __wasm__
#include <stdlib.h>
int main() {
	FILE* f = fopen("a.S", "r");
	FILE* out = fopen("a.bin", "wb");

	fseek(f, 0, SEEK_END);
	size_t s = ftell(f);
	rewind(f);
	char* txt = malloc(s);
	fread(txt, s, 1, f);
    assemble(txt, s);
	printf("assembled %d\n", asm_emit_idx);
	fwrite(ram, asm_emit_idx, 1, out);
}
#endif
