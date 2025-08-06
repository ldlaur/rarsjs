#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "types.h"

#define export __attribute__((visibility("default")))

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
extern void shadowstack_push();
extern void shadowstack_pop();
#define assert(cond)          \
    {                         \
        if (!(cond)) panic(); \
    }
#else

static inline void shadowstack_push() {}
static inline void shadowstack_pop() {}

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define emu_exit() g_exited = true
#endif

#define TEXT_BASE 0x00400000
#define TEXT_END 0x10000000
#define DATA_BASE 0x10000000
#define STACK_TOP 0x7FFFF000
#define STACK_LEN 4096
#define DATA_END 0x70000000

#define push(arr, len, cap)                                           \
    ((len) >= (cap)  ? grow((void **)&(arr), &(cap), sizeof(*(arr))), \
     (arr) + (len)++ : (arr) + (len)++)

static void grow(void **arr, size_t *cap, size_t size) {
    size_t oldcap = *cap;
    if (oldcap) *cap = oldcap * 2;
    else *cap = 4;
    void *newarr = malloc(*cap * size);
    memset(newarr, 0, *cap * size);
    if (arr) {
        if (*arr) {
            memcpy(newarr, *arr, oldcap * size);
            free(*arr);
        }
        *arr = newarr;
    }
}

typedef struct Parser {
    const char *input;
    size_t pos;
    size_t size;
    int lineidx;
    int startline;
} Parser;

typedef struct {
    const char *symbol;
    size_t len;
    struct {
        size_t stidx;
    } elf;
} Extern;

typedef struct {
    size_t offset;
    size_t size;
    size_t addend;
    Extern *symbol;
    size_t type;
} Relocation;

typedef struct Section {
    const char *name;
    u32 base;
    u32 limit;
    size_t len;
    size_t capacity;
    u8 *buf;
    size_t emit_idx;
    u32 align;
    struct {
        Relocation *buf;
        size_t len;
        size_t cap;
    } relocations;
    struct {
        size_t shidx;
    } elf;
    bool read;
    bool write;
    bool execute;
    bool physical;
} Section;

typedef struct LabelData {
    const char *txt;
    size_t len;
    u32 addr;
    Section *section;
} LabelData;

typedef const char *DeferredInsnCb(Parser *p, const char *opcode,
                                   size_t opcode_len);
typedef const char *DeferredInsnReloc(const char *sym, size_t sym_len);

typedef struct DeferredInsn {
    Parser p;
    Section *section;
    DeferredInsnCb *cb;
    DeferredInsnReloc *reloc;
    const char *opcode;
    size_t opcode_len;
    size_t emit_idx;
} DeferredInsn;

typedef struct Global {
    const char *str;
    size_t len;
    struct {
        size_t stidx;
    } elf;
} Global;

typedef enum Error : u32 {
    ERROR_NONE = 0,
    ERROR_FETCH = 1,
    ERROR_LOAD = 2,
    ERROR_STORE = 3,
    ERROR_UNHANDLED_INSN = 4,
    ERROR_CALLSAN_CANTREAD = 5,
    ERROR_CALLSAN_NOT_SAVED = 6,
    ERROR_CALLSAN_SP_MISMATCH = 7,
    ERROR_CALLSAN_RA_MISMATCH = 8,
    ERROR_CALLSAN_RET_EMPTY = 9,
    ERROR_CALLSAN_LOAD_STACK = 10
} Error;

extern export Section g_text;
extern export Section g_data;
extern export Section g_stack;

extern export Section **g_sections;
extern export size_t g_sections_len, g_sections_cap;

extern export u32 g_regs[32];
extern export u32 g_pc;

extern LabelData *g_labels;
extern size_t g_labels_len, g_labels_cap;

extern Global *g_globals;
extern size_t g_globals_len, g_globals_cap;

extern Extern *g_externs;
extern size_t g_externs_len, g_externs_cap;

extern export u32 g_error_line;
extern export const char *g_error;

extern export u32 g_runtime_error_params[2];
extern export Error g_runtime_error_type;

extern export bool g_exited;
extern export int g_exit_code;

extern export u32 *g_text_by_linenum;
extern export size_t g_text_by_linenum_len, g_text_by_linenum_cap;

extern const char *const REGISTER_NAMES[];

void assemble(const char *str, size_t len, bool allow_externs);
void emulate();
bool resolve_symbol(const char *sym, size_t sym_len, bool global, u32 *addr,
                    Section **sec);
void prepare_runtime_sections();
void prepare_stack();
void free_runtime();
u32 LOAD(u32 addr, int size, bool *err);
bool pc_to_label_r(u32 pc, LabelData **ret, u32 *off);

enum Reg {
    REG_ZERO = 0,
    REG_RA,
    REG_SP,
    REG_GP,
    REG_TP,
    REG_T0,
    REG_T1,
    REG_T2,
    REG_FP,
    REG_S1,
    REG_A0,
    REG_A1,
    REG_A2,
    REG_A3,
    REG_A4,
    REG_A5,
    REG_A6,
    REG_A7,
    REG_S2,
    REG_S3,
    REG_S4,
    REG_S5,
    REG_S6,
    REG_S7,
    REG_S8,
    REG_S9,
    REG_S10,
    REG_S11,
    REG_T3,
    REG_T4,
    REG_T5,
    REG_T6
};
