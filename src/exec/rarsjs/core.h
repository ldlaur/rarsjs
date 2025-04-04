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

#define assert(cond)          \
    {                         \
        if (!(cond)) panic(); \
    }
#else

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

#define push(arr, len, cap) \
    ((len) >= (cap) ? grow((void **)&(arr), &(cap), sizeof(*(arr))), (arr) + (len)++ : (arr) + (len)++)

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

typedef struct LabelData {
    const char *txt;
    size_t len;
    u32 addr;
} LabelData;

typedef struct Section {
    const char *name;
    u32 base;
    u32 limit;
    size_t len;
    size_t capacity;
    u8 *buf;
    size_t emit_idx;
    u32 align;
    bool read;
    bool write;
    bool execute;
    bool physical;
} Section;

typedef const char *DeferredInsnCb(Parser *p, const char *opcode, size_t opcode_len);

typedef struct DeferredInsn {
    Parser p;
    Section *section;
    DeferredInsnCb *cb;
    const char *opcode;
    size_t opcode_len;
    size_t emit_idx;
} DeferredInsn;

typedef struct Global {
    const char *str;
    size_t len;
} Global;

typedef enum Error : u32 { ERROR_NONE = 0, ERROR_FETCH = 1, ERROR_LOAD = 2, ERROR_STORE = 3 } Error;

extern export Section g_text;
extern export Section g_data;
extern export Section g_stack;

extern export Section **g_sections;
extern export size_t g_sections_len, g_sections_cap;

extern export u32 g_regs[32];
extern export u32 g_pc;

extern LabelData *g_labels;
extern size_t g_labels_len, g_labels_cap;

extern export u32 g_error_line;
extern export const char *g_error;

extern export u32 g_runtime_error_addr;
extern export Error g_runtime_error_type;

extern export bool g_exited;
extern export int g_exit_code;

void assemble(const char *, size_t);
void emulate();
bool resolve_symbol(const char *sym, size_t sym_len, bool global, u32* addr); 
void prepare_runtime_sections();
void free_runtime();