#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef uint32_t u32;

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

void skip_whitespace(char* txt, size_t* ti, size_t s) {
	while (whitespace(txt[*ti]) && *ti < s) (*ti)++;
}

void parse_opcode(char** str, size_t* len, char* txt, size_t* ti, size_t s) {
	size_t st_opc = *ti;
	while (isalnum(txt[*ti]) && *ti < s) (*ti)++;
	size_t en_opc = *ti;
	*str = txt + st_opc;
	*len = en_opc - st_opc;
}

bool str_eq(char* txt, size_t len, const char* c) {
	if (len != strlen(c)) return false;
	return memcmp(txt, c, len) == 0;
}

void parse_reg(char** str, size_t* len, char* txt, size_t* ti, size_t s) {
	size_t st = *ti;
	while (isalnum(txt[*ti]) && *ti < s) (*ti)++;
	size_t end = *ti;
	*str = txt + st;
	*len = end - st;
}

void parse_simm(char** str, size_t* len, char* txt, size_t* ti, size_t s) {
	size_t st = *ti;
    if (txt[*ti] == '-' && *ti < s) (*ti)++;
	while (isdigit(txt[*ti]) && *ti < s) (*ti)++;
	size_t end = *ti;
	*str = txt + st;
	*len = end - st;
}

int regname_to_num(char* str, size_t len) {
    if (len != 2 && len != 3) return -1;
    if (str[0] == 'x') {
        if (len == 2) return str[1] - '0';
        else {
            int num = (str[2]-'0')*10 + (str[1]-'0');
            if (num >= 32) return -1;
            return num;
        }
    }
    // TODO: aliases
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

void handle_alu_reg(char* opcode, size_t opcode_len, char* txt, size_t* ti_, size_t s) {
	size_t ti = *ti_;

    char *rd, *rs1, *rs2;
    size_t rd_len, rs1_len, rs2_len;
    
	skip_whitespace(txt, &ti, s);
    parse_reg(&rd, &rd_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);
	assert(txt[ti] == ','); ti++;

	skip_whitespace(txt, &ti, s);
    parse_reg(&rs1, &rs1_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);
	assert(txt[ti] == ','); ti++;

	skip_whitespace(txt, &ti, s);
    parse_reg(&rs2, &rs2_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);
    
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
    printf("%x\n", inst);

	*ti_ = ti;
}

void handle_alu_imm(char* opcode, size_t opcode_len, char* txt, size_t* ti_, size_t s) {
	size_t ti = *ti_;
	skip_whitespace(txt, &ti, s);

    char *rd, *rs1, *imm;
    size_t rd_len, rs1_len, imm_len;
    
	skip_whitespace(txt, &ti, s);
    parse_reg(&rd, &rd_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);
	assert(txt[ti] == ','); ti++;

	skip_whitespace(txt, &ti, s);
    parse_reg(&rs1, &rs1_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);
	assert(txt[ti] == ','); ti++;

	skip_whitespace(txt, &ti, s);
    parse_simm(&imm, &imm_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);

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
    printf("%x\n", inst);

	*ti_ = ti;
}

void handle_ldst(char* opcode, size_t opcode_len, char* txt, size_t* ti_, size_t s) {
	size_t ti = *ti_;
	skip_whitespace(txt, &ti, s);

    char *rreg, *rmem, *imm;
    size_t rreg_len, rmem_len, imm_len;
    
	skip_whitespace(txt, &ti, s);
    parse_reg(&rreg, &rreg_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);
	assert(txt[ti] == ','); ti++;

	skip_whitespace(txt, &ti, s);
    parse_simm(&imm, &imm_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);

	assert(txt[ti] == '('); ti++;
	
    skip_whitespace(txt, &ti, s);
    parse_reg(&rmem, &rmem_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);

	assert(txt[ti] == ')'); ti++;

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

    printf("%x\n", inst);

	*ti_ = ti;
}


void handle_branch(char* opcode, size_t opcode_len, char* txt, size_t* ti_, size_t s) {
	size_t ti = *ti_;
	skip_whitespace(txt, &ti, s);

    char *rd, *rs1, *imm;
    size_t rd_len, rs1_len, imm_len;
    
	skip_whitespace(txt, &ti, s);
    parse_reg(&rd, &rd_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);
	assert(txt[ti] == ','); ti++;

	skip_whitespace(txt, &ti, s);
    parse_reg(&rs1, &rs1_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);
	assert(txt[ti] == ','); ti++;

	skip_whitespace(txt, &ti, s);
    parse_simm(&imm, &imm_len, txt, &ti, s);
	skip_whitespace(txt, &ti, s);

    int d, s1, simm;
    d = regname_to_num(rd, rd_len);
    assert(d != -1);
    s1 = regname_to_num(rs1, rs1_len);
    assert(s1 != -1);
    simm = atoi_len(imm, imm_len);

    u32 inst = 0;
    if      (str_eq(opcode, opcode_len, "beq")) inst = BEQ (d, s1, simm);
    else if (str_eq(opcode, opcode_len, "bne")) inst = BNE (d, s1, simm);
    else if (str_eq(opcode, opcode_len, "blt")) inst = BLT (d, s1, simm);
    else if (str_eq(opcode, opcode_len, "bge")) inst = BGE (d, s1, simm);
    else if (str_eq(opcode, opcode_len, "bltu"))inst = BLTU(d, s1, simm);
    else if (str_eq(opcode, opcode_len, "bgeu"))inst = BGEU(d, s1, simm);
    printf("%x\n", inst);

	*ti_ = ti;
}


void handle_jumps(char* opcode, size_t opcode_len, char* txt, size_t* ti_, size_t s) {
    size_t ti = *ti_;
    skip_whitespace(txt, &ti, s);

    char *rd, *rs1, *imm;
    size_t rd_len, rs1_len, imm_len;

    skip_whitespace(txt, &ti, s);
    parse_reg(&rd, &rd_len, txt, &ti, s);
    skip_whitespace(txt, &ti, s);
    assert(txt[ti] == ','); ti++;

    skip_whitespace(txt, &ti, s);
    
    int d, s1, simm;
    u32 inst = 0;

    if (str_eq(opcode, opcode_len, "jal")) {
        parse_simm(&imm, &imm_len, txt, &ti, s);
        simm = atoi_len(imm, imm_len);
        d = regname_to_num(rd, rd_len);
        assert(d != -1);
        inst = JAL(d, simm);
    } 
    else if (str_eq(opcode, opcode_len, "jalr")) {
        parse_reg(&rs1, &rs1_len, txt, &ti, s);
        skip_whitespace(txt, &ti, s);
        assert(txt[ti] == ','); ti++;
        skip_whitespace(txt, &ti, s);

        parse_simm(&imm, &imm_len, txt, &ti, s);
        simm = atoi_len(imm, imm_len);
        d = regname_to_num(rd, rd_len);
        s1 = regname_to_num(rs1, rs1_len);
        assert(d != -1 && s1 != -1);
        inst = JALR(d, s1, simm);
    } 

    printf("%x\n", inst);

    *ti_ = ti;
}


void handle_upper(char* opcode, size_t opcode_len, char* txt, size_t* ti_, size_t s) {
    size_t ti = *ti_;
    skip_whitespace(txt, &ti, s);

    char *rd, *rs1, *imm;
    size_t rd_len, rs1_len, imm_len;

    skip_whitespace(txt, &ti, s);
    parse_reg(&rd, &rd_len, txt, &ti, s);
    skip_whitespace(txt, &ti, s);
    assert(txt[ti] == ','); ti++;

    skip_whitespace(txt, &ti, s);
    
    int d, s1, simm;
    u32 inst = 0;

    parse_simm(&imm, &imm_len, txt, &ti, s);
    simm = atoi_len(imm, imm_len);
    d = regname_to_num(rd, rd_len);
    assert(d != -1);

    if (str_eq(opcode, opcode_len, "lui")) inst = LUI(d, simm);
    else if (str_eq(opcode, opcode_len, "auipc")) inst = AUIPC(d, simm);

    printf("%x\n", inst);

    *ti_ = ti;
}


void handle_upper(char* opcode, size_t opcode_len, char* txt, size_t* ti_, size_t s) {
    size_t ti = *ti_;
    skip_whitespace(txt, &ti, s);

    char *rd, *rs1, *imm;
    size_t rd_len, rs1_len, imm_len;

    skip_whitespace(txt, &ti, s);
    parse_reg(&rd, &rd_len, txt, &ti, s);
    skip_whitespace(txt, &ti, s);
    assert(txt[ti] == ','); ti++;

    skip_whitespace(txt, &ti, s);
    
    int d, s1, simm;
    u32 inst = 0;

    parse_simm(&imm, &imm_len, txt, &ti, s);
    simm = atoi_len(imm, imm_len);
    d = regname_to_num(rd, rd_len);
    assert(d != -1);

    if (str_eq(opcode, opcode_len, "lui")) inst = LUI(d, simm);
    else if (str_eq(opcode, opcode_len, "auipc")) inst = AUIPC(d, simm);

    printf("%x\n", inst);

    *ti_ = ti;
}


void handle_ecall() {
    printf("%x\n", 0x73);
}


int main() {
	FILE* f = fopen("a.S", "r");
	fseek(f, 0, SEEK_END);
	size_t s = ftell(f);
	rewind(f);
	char* txt = malloc(s);
	fread(txt, s, 1, f);

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


	while (1) {
        if (ti == s) return 0;

		skip_whitespace(txt, &ti, s);
		char* opcode;
		size_t opcode_len;
		parse_opcode(&opcode, &opcode_len, txt, &ti, s);
        bool found = false;
        for (int i = 0; !found && i < sizeof(opcodes_alu_imm)/sizeof(char*); i++) {
			if (str_eq(opcode, opcode_len, opcodes_alu_imm[i])) {
				handle_alu_imm(opcode, opcode_len, txt, &ti, s);
                found = true;
            }
		}
		for (int i = 0; !found && i < sizeof(opcodes_alu_reg)/sizeof(char*); i++) {
			if (str_eq(opcode, opcode_len, opcodes_alu_reg[i])) {
				handle_alu_reg(opcode, opcode_len, txt, &ti, s);
                found = true;
            }
		}
		
        for (int i = 0; !found && i < sizeof(opcodes_ldst)/sizeof(char*); i++) {
			if (str_eq(opcode, opcode_len, opcodes_ldst[i])) {
				handle_ldst(opcode, opcode_len, txt, &ti, s);
                found = true;
            }
		}

        for (int i = 0; !found && i < sizeof(opcodes_branch)/sizeof(char*); i++) {
			if (str_eq(opcode, opcode_len, opcodes_branch[i])) {
				handle_branch(opcode, opcode_len, txt, &ti, s);
                found = true;
            }
		}

        if (str_eq(opcode, opcode_len, "jal") || str_eq(opcode, opcode_len, "jalr")) {
            handle_jumps(opcode, opcode_len, txt, &ti, s);
            found = true;
        }

        if (str_eq(opcode, opcode_len, "lui") || str_eq(opcode, opcode_len, "auipc")) {
            handle_upper(opcode, opcode_len, txt, &ti, s);
            found = true;
        }

        if (str_eq(opcode, opcode_len, "li")) {
            handle_li(opcode, opcode_len, txt, &ti, s);
            found = true;
        }

        if (str_eq(opcode, opcode_len, "ecall")) {
            handle_ecall();
            found = true;
        }

        assert(found);
	}
}




