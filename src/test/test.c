#include <unity.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../exec/rarsjs/core.h"

void setUp(void) {}
void tearDown(void) {
    free_runtime();
}

// need this wrapper because TEST_ASSERT_EQUAL_STRING_LEN doesn't check that the length matches
#define TEST_ASSERT_EQUAL_STR(x, y, z) { TEST_ASSERT_EQUAL(strlen(x), z); TEST_ASSERT_EQUAL_STRING_LEN(x, y, z); }

static Parser init_parser(const char *str) {
    Parser p;
    p.input = str;
    p.pos = 0;
    p.size = strlen(str);
    p.lineidx = 1;
    return p;
}

void test_parse_numeric_decimal(void) {
    Parser p = init_parser("+-+-123");
    int result = 0;
    bool ok = parse_numeric(&p, &result);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(123, result);
}

void test_parse_numeric_invalid(void) {
    Parser p = init_parser("0x");
    int result = 0;
    bool ok = parse_numeric(&p, &result);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT(0, p.pos);
}

void test_invalid_literals(void) {
    Parser p = init_parser("0b102");
    int result = 0;
    bool ok = parse_numeric(&p, &result);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_numeric_unterminated_char(void) {
    Parser p = init_parser("'a");
    int result = 0;
    bool ok = parse_numeric(&p, &result);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_quoted_str_valid(void) {
    char *out = NULL;
    size_t out_len = 0;
    Parser p = init_parser("\"hello\\n\"");
    bool ok = parse_quoted_str(&p, &out, &out_len);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STR("hello\n", out, out_len);
}

void test_parse_quoted_str_unterminated(void) {
    char *out = NULL;
    size_t out_len = 0;
    Parser p = init_parser("\"unterminated");
    bool ok = parse_quoted_str(&p, &out, &out_len);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_quoted_harder(void) {
    char *out = NULL;
    size_t out_len = 0;
    Parser p = init_parser("\"printf(\\\"Hello\\\")\"");
    bool ok = parse_quoted_str(&p, &out, &out_len);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STR("printf(\"Hello\")", out, out_len);
}

void test_parse_quoted_backslash(void) {
    Parser p = init_parser("\"C:\\\\Users\\\\\"");
    char *out = NULL;
    size_t out_len = 0;
    bool ok = parse_quoted_str(&p, &out, &out_len);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STR("C:\\Users\\", out, out_len);
}

void test_skip_comment_line_invaid(void) {
    Parser p = init_parser("/a this is invalid");
    bool skipped = skip_comment(&p);
    TEST_ASSERT_FALSE(skipped);
}

void test_skip_comment_multiline2(void) {
    Parser p = init_parser("/* nonterminated *");
    bool skipped = skip_comment(&p);
    TEST_ASSERT_TRUE(skipped);
    TEST_ASSERT_EQUAL(p.pos, p.size);
}

void test_skip_comment_block(void) {
    Parser p = init_parser("/* block comment */123");
    bool skipped = skip_comment(&p);
    TEST_ASSERT_TRUE(skipped);
    TEST_ASSERT_EQUAL('1', p.input[p.pos]);
}

void test_skip_whitespace(void) {
    Parser p = init_parser("   \n//comment\n  \t789");
    skip_whitespace(&p);
    TEST_ASSERT_TRUE(p.pos < p.size);
    TEST_ASSERT_EQUAL('7', p.input[p.pos]);
}

void test_invalid_literal(void) {
    Parser p = init_parser("-abc");
    int result = 0;
    bool ok = parse_numeric(&p, &result);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_quoted_str_invalid_escape(void) {
    Parser p = init_parser("\"hello\\x\"");
    char *out = NULL;
    size_t out_len = 0;
    bool ok = parse_quoted_str(&p, &out, &out_len);
    TEST_ASSERT_FALSE(ok);
}

void assemble_line(const char *line) {
    assemble(line, strlen(line), false);
}

void test_unknown_opcode(void) {
    assemble_line("unhandled");
    TEST_ASSERT_EQUAL_STRING(g_error, "Unknown opcode");
}

void test_unterminated_instruction(void) {
    assemble_line("addi x1, x2 ");
    TEST_ASSERT_EQUAL_STRING(g_error, "Expected ,");
}

void test_addi_immediate_out_of_range(void) {
    assemble_line("addi x1, x2, 3000");
    TEST_ASSERT_EQUAL_STRING(g_error, "Out of bounds imm");
}

void test_lui_immediate_out_of_range(void) {
    assemble_line("lui x1, 1048576");
    TEST_ASSERT_EQUAL_STRING(g_error, "Out of bounds imm");
}

void test_sw_immediate_out_of_range(void) {
    assemble_line("sw x1, 5000(x2)");
    TEST_ASSERT_EQUAL_STRING(g_error, "Out of bounds imm");
}

void test_sw_immediate_newline(void) {
    assemble_line("sw x1, 1000\n(x2)"); 
    TEST_ASSERT_EQUAL_STRING(g_error, NULL);
}

void test_addi_immediate_newline_after_comma(void) {
    assemble_line("addi x1, x2,\n300");
    TEST_ASSERT_EQUAL_STRING(g_error, NULL);
}

void test_sw_immediate_multiple_newlines(void) {
    assemble_line("sw x1, 1000\n\n(x2)");
    TEST_ASSERT_EQUAL_STRING(g_error, NULL);
}

void test_sw_register_newline(void) {
    assemble_line("sw x1, 1000(x\n2)");
    TEST_ASSERT_EQUAL_STRING(g_error, "Invalid rmem");
}

void test_sw_register_newline2(void) {
    assemble_line("sw x1, 1000(x2\n)");
    TEST_ASSERT_EQUAL_STRING(g_error, NULL);
}

void test_addi_immediate_with_spaces_newlines(void) {
    assemble_line("addi x1 ,\nx2,  -2048");
    TEST_ASSERT_EQUAL_STRING(g_error, NULL);
}

void test_instruction_trailing_comma(void) {
    assemble_line("addi x1, x2, 300,");
    TEST_ASSERT_EQUAL_STRING(g_error, "Expected newline");
}

void test_sw_newline(void) {
    assemble_line("sw\n x1\n,\n1000( x2)");
    TEST_ASSERT_EQUAL_STRING(g_error, NULL);
}

void test_addi_oob(void) {
    assemble_line("addi x1, x2, 2048");
    TEST_ASSERT_EQUAL_STRING(g_error, "Out of bounds imm");
    free_runtime();

    assemble_line("addi x1, x2, -2049");
    TEST_ASSERT_EQUAL_STRING(g_error, "Out of bounds imm");
}

void test_lui_oob(void) {
    assemble_line("lui x1, 0x100000");
    TEST_ASSERT_EQUAL_STRING(g_error, "Out of bounds imm");
}

void test_add_invalid_reg(void) {
    assemble_line("add x1, x2, x32");
    TEST_ASSERT_EQUAL_STRING(g_error, "Invalid rs2");
}

void test_case_insensitivity(void) {
    assemble_line("ADDI X1, X2, 0X41");
    TEST_ASSERT_EQUAL_STRING(g_error, NULL);
}

void test_instruction_with_trailing_garbage(void) {
    assemble_line("addi x1, x2, 1000 garbage");
    TEST_ASSERT_EQUAL_STRING(g_error, "Expected newline");
}

void test_parse_directives_nums() {
    // TODO: make tests endianness independent
    u32 word;
    assemble_line(".data\nvar: .WORD 5");
    TEST_ASSERT_EQUAL_INT(g_data->contents.len, 4);
    memcpy(&word, g_data->contents.buf, 4);
    TEST_ASSERT_EQUAL_INT(word, 5);
    free_runtime();

    u16 half; 
    assemble_line(".DATA\nvar: .HALF 5");
    TEST_ASSERT_EQUAL_INT(g_data->contents.len, 2);
    memcpy(&half, g_data->contents.buf, 2);
    TEST_ASSERT_EQUAL_INT(half, 5);
    free_runtime();

    u8 byte;
    assemble_line(".data\nvar: .byte 5");
    TEST_ASSERT_EQUAL_INT(g_data->contents.len, 1);
    memcpy(&byte, g_data->contents.buf, 1);
    TEST_ASSERT_EQUAL_INT(byte, 5);
}

void test_parse_directives_nums_invalid() {
    assemble_line(".data\nvar: .word 0xG");
    TEST_ASSERT_EQUAL_STRING(g_error, "Invalid word");
}

void test_parse_directives_nums_oob() {
    assemble_line(".data\nvar: .half 0x10000");
    TEST_ASSERT_EQUAL_STRING(g_error, "Out of bounds half");
    free_runtime();

    assemble_line(".data\nvar: .half -32769");
    TEST_ASSERT_EQUAL_STRING(g_error, "Out of bounds half");
    free_runtime();

    assemble_line(".data\nvar: .byte 0x100");
    TEST_ASSERT_EQUAL_STRING(g_error, "Out of bounds byte");
    free_runtime();

    assemble_line(".data\nvar: .byte -129");
    TEST_ASSERT_EQUAL_STRING(g_error, "Out of bounds byte");
    free_runtime();
}

void test_parse_directives_str() {
    assemble_line(".data\nstr: .ASCII \"hi\", \"hi\"");
    TEST_ASSERT_EQUAL_STR("hihi", g_data->contents.buf, g_data->contents.len);
    free_runtime();

    assemble_line(".data\nstr: .string \"hi\"");
    TEST_ASSERT_EQUAL_INT(g_data->contents.len, 3);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("hi\0", g_data->contents.buf, g_data->contents.len);
    free_runtime();
}

void test_parse_multiple_definitions() {
    assemble_line(".data\nvar: .word 5\nvar: .word 10");
    TEST_ASSERT_EQUAL_STRING(g_error, "Multiple definitions for the same label");
}

void test_parse_symbol_resolution_error() {
    assemble_line("j unknown_symbol\n");
    TEST_ASSERT_EQUAL_STRING("Label not found", g_error);
}

void test_resolve_text_symbol_found(void) {
    assemble_line("mylabel: addi a0, a0, 0");
    u32 addr;
    Section *sec;
    bool found = resolve_symbol("mylabel", strlen("mylabel"), false, &addr, &sec);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL(sec, g_text);
    TEST_ASSERT_TRUE(addr >= g_text->base && addr < g_text->limit);
}

void test_resolve_data_symbol_found(void) {
    assemble_line(".data\nmylabel: .word 1234");
    u32 addr;
    Section *sec;
    bool found = resolve_symbol("mylabel", strlen("mylabel"), false, &addr, &sec);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL(sec, g_data);
    TEST_ASSERT_TRUE(addr >= g_data->base && addr < g_data->limit);
}

void test_pc_to_label_r2(void) {
    assemble_line("label: add x0, x0, x0");
    LabelData *ret = NULL;
    u32 off = 0;
    bool result = pc_to_label_r(g_text->base, &ret, &off);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STR("label", ret->txt, ret->len);
}

void test_pc_to_label_r_no_label(void) {
    assemble_line("add x0, x0, x0");
    LabelData *ret = NULL;
    u32 off = 0;
    bool result = pc_to_label_r(0xdeadbeef, &ret, &off);
    TEST_ASSERT_FALSE(result);
}

void test_fixup(void) {
    assemble_line("j exit\nexit:");
    bool err;
    TEST_ASSERT_EQUAL_INT(LOAD(g_text->base, 4, &err), 0x0040006f);
    TEST_ASSERT_FALSE(err);
}

void test_backtrack(void) {
    assemble_line("j .exit\n.exit:");
    TEST_ASSERT_EQUAL_STRING(g_error, NULL);
    bool err;
    TEST_ASSERT_EQUAL_INT(LOAD(g_text->base, 4, &err), 0x0040006f);
    TEST_ASSERT_FALSE(err);
}

void test_dotlabel_fail(void) {
    assemble_line("j .data\n.data:");
    TEST_ASSERT_EQUAL_STRING(g_error, "Label not found");
}

void test_nolabel(void) {
    assemble_line("j ");
    TEST_ASSERT_EQUAL_STRING(g_error, "No label");
}

void test_nonglobal_start(void) {
    assemble_line("_start: ");
    TEST_ASSERT_EQUAL_STRING(g_error, "_start defined, but without .globl");
}

void test_start_in_data(void) {
    assemble_line(".globl _start\n.data\n_start: ");
    TEST_ASSERT_EQUAL_STRING(g_error, "_start not in .text section");
}

// -- runtime tests

void build_and_run(const char* txt) {
    u32 addr;
    assemble(txt, strlen(txt), false);
    TEST_ASSERT_EQUAL_STRING(g_error, NULL);
    if (resolve_symbol("_start", strlen("_start"), true, &addr, NULL)) g_pc = addr;
    while (!g_exited) {
        emulate();
        if (g_runtime_error_type != ERROR_NONE) break;
    }
}
void check_pc_at_label(const char* label) {
    u32 addr;
    TEST_ASSERT_TRUE(resolve_symbol(label, strlen(label), false, &addr, NULL));
    TEST_ASSERT_EQUAL(g_pc, addr);
}
void test_runtime_exit() {
    build_and_run("li a7, 93\necall");
    TEST_ASSERT_EQUAL(g_runtime_error_type, ERROR_NONE);
}

// 16bit instruction, currently unhandled
void test_runtime_unhandled() {
    build_and_run("\
.globl _start   \n\
_start:         \n\
E:  .word 0b01  \n\
");
    TEST_ASSERT_EQUAL(g_runtime_error_type, ERROR_UNHANDLED_INSN);
    check_pc_at_label("E");
}

void test_callsan_cantread() {
    build_and_run("\
fn:                \n\
    ret            \n\
.globl _start      \n\
_start:            \n\
    li a3, 2       \n\
    jal fn         \n\
E:  addi a3, a3, 1 \n\
");
    TEST_ASSERT_EQUAL(g_runtime_error_type, ERROR_CALLSAN_CANTREAD);
    TEST_ASSERT_EQUAL(g_runtime_error_params[0], REG_A3);
    check_pc_at_label("E");
}

void test_callsan_not_saved() {
    build_and_run("\
fn:             \n\
    li s1, 1234 \n\
E:  ret         \n\
.globl _start   \n\
_start:         \n\
    jal fn      \n\
");
    TEST_ASSERT_EQUAL(g_runtime_error_type, ERROR_CALLSAN_NOT_SAVED);
    TEST_ASSERT_EQUAL(g_runtime_error_params[0], REG_S1);
    check_pc_at_label("E");
}

void test_callsan_ra_mismatch() {
    build_and_run("\
fn2:                 \n\
    ret              \n\
fn:                  \n\
    jal fn2          \n\
E:  ret              \n\
.globl _start        \n\
_start:              \n\
    jal fn           \n\
");
    TEST_ASSERT_EQUAL(g_runtime_error_type, ERROR_CALLSAN_RA_MISMATCH);
    check_pc_at_label("E");
}

void test_callsan_sp_mismatch() {
    build_and_run("\
fn:                  \n\
    addi sp, sp, -16 \n\
    addi sp, sp, 24  \n\
E:  ret              \n\
.globl _start        \n\
_start:              \n\
    jal fn           \n\
    li a7, 93        \n\
    ecall            \n\
");
    TEST_ASSERT_EQUAL(g_runtime_error_type, ERROR_CALLSAN_SP_MISMATCH);
    check_pc_at_label("E");
}

void test_callsan_ret_empty() {
    build_and_run("\
fn:                  \n\
    addi sp, sp, -16 \n\
    addi sp, sp, 16  \n\
    ret              \n\
.globl _start        \n\
_start:              \n\
    jal fn           \n\
E:  ret              \n\
");
    TEST_ASSERT_EQUAL(g_runtime_error_type, ERROR_CALLSAN_RET_EMPTY);
    check_pc_at_label("E");
}

// first set of accesses should be valid, second no
void test_callsan_load_stack() {
    build_and_run("\
fn:                 \n\
    addi sp, sp, -8 \n\
    sw ra, 0(sp)    \n\
    lw ra, 0(sp)    \n\
E:  lw ra, 4(sp)    \n\
    sw ra, 4(sp)    \n\
    addi sp, sp, 8  \n\
    ret             \n\
.globl _start       \n\
_start:             \n\
    jal fn          \n\
");
    TEST_ASSERT_EQUAL(g_runtime_error_type, ERROR_CALLSAN_LOAD_STACK);
    check_pc_at_label("E");
}