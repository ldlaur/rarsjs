#pragma once

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

#define emu_exit() exit(0)
#endif

#define TEXT_BASE 0x00400000
#define TEXT_END 0x10000000
#define DATA_BASE 0x10000000
#define STACK_TOP 0x7FFFF000
#define DATA_END 0x80000000

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

typedef const char *DeferredInsnCb(Parser *p, const char *opcode, size_t opcode_len);

typedef struct DeferredInsn {
    Parser p;
    Section section;
    DeferredInsnCb *cb;
    const char *opcode;
    size_t opcode_len;
    size_t emit_idx;
} DeferredInsn;

typedef enum Error : u32 { ERROR_NONE = 0, ERROR_FETCH = 1, ERROR_LOAD = 2, ERROR_STORE = 3 } Error;

extern export u8 *g_text;
extern export size_t g_text_len, g_text_cap;
extern size_t g_text_emit_idx;
extern export u8 *g_data;
extern export size_t g_data_len, g_data_cap;
extern size_t g_data_emit_idx;

extern export u32 g_regs[32];
extern export u32 g_pc;

extern LabelData *g_labels;
extern size_t g_labels_len, g_labels_cap;

extern export u32 g_error_line;
extern export const char *g_error;

extern export u32 g_runtime_error_addr;
extern export Error g_runtime_error_type;

void assemble(const char *, size_t);
void emulate();
