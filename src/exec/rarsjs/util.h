#pragma once

#include <stddef.h>
#ifdef __wasm__
void *malloc(size_t size);
void free(void *ptr);
extern void panic();
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *dest, int c, size_t n);

#define RARSJS_CHECK_OOM(ptr)               \
    if (!(ptr)) {                           \
        panic(); \
    }
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RARSJS_CHECK_OOM(ptr)               \
    if (!(ptr)) {                           \
        fprintf(stderr, "out of memory\n"); \
        exit(137);                          \
    }
#endif

#define RARSJS_ARRAY_PUSH(arr)                                 \
    (((arr)->len) >= ((arr)->cap)                               \
     ? (arr)->buf = rarsjs_array_grow((arr)->buf, &((arr)->cap), \
                         sizeof(*((arr)->buf))),                \
     (arr)->buf + ((arr)->len)++ : (arr)->buf + ((arr)->len)++)

#define RARSJS_ARRAY_DEF(type) \
    typedef struct {           \
        size_t len;            \
        size_t cap;            \
        type *buf;             \
    }

#define RARSJS_ARRAY_TYPE(type) RARSJS_ARRAY_DEF(type) Array_##type

#define RARSJS_ARRAY(alias) Array_##alias
#define RARSJS_ARRAY_INIT(arr) (arr)->len = 0, (arr)->cap = 0
#define RARSJS_ARRAY_NEW(type) (RARSJS_ARRAY(type)){.buf = NULL, .len = 0, .cap = 0}
#define RARSJS_ARRAY_FREE(arr) free((arr)->buf), (arr)->buf = NULL, (arr)->len = (arr)->cap = 0
#define RARSJS_ARRAY_INSERT(arr, pos)                           \
    ((pos) >= ((arr)->cap)                                      \
     ? rarsjs_array_grow((void **)&((arr)->buf), &((arr)->cap), \
                         sizeof(*((arr)->buf))),                \
     (arr)->buf + (pos) : (arr)->buf + (pos))
#define RARSJS_ARRAY_LEN(arr) (arr)->len
#define RARSJS_ARRAY_CAP(arr) (arr)->cap
#define RARSJS_ARRAY_GET(arr, pos) (&(arr)->buf[pos])
#define RARSJS_ARRAY_PREPARE(type, cap) \
    (RARSJS_ARRAY(type)) { (cap), (cap), NULL }
#define RARSJS_ARRAY_POP(arr) &((arr)->buf[--(arr)->len])

#define RARSJS_CHECK_CALL(expr, fail_label) \
    if (!(expr)) {                          \
        goto fail_label;                    \
    }

static void* rarsjs_array_grow(void *arr, size_t *cap, size_t size) {
    size_t oldcap = *cap;
    if (oldcap) *cap = oldcap * 2;
    else *cap = 4;
    void *newarr = malloc(*cap * size);
    RARSJS_CHECK_OOM(newarr);
    memset(newarr, 0, *cap * size);
    if (arr) {
        memcpy(newarr, arr, oldcap * size);
        free(arr);
    }
    return newarr;
}
