#ifdef __wasm__
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern char __heap_base;
size_t g_heap_size = 0;
extern void panic();
void *malloc(size_t size) {
    size_t bytes = __builtin_wasm_memory_size(0) << 16;
    if ((g_heap_size + size) > bytes) {
        size_t pages = (g_heap_size + size - bytes + 65535) >> 16;
        __builtin_wasm_memory_grow(0, pages);
    }
    void *alloc = (&__heap_base) + g_heap_size;
    g_heap_size += size;
    return alloc;
}

void free(void *ptr) {
    // lol
}

size_t strlen(const char *str) {
    const char *s;
    for (s = str; *s; ++s);
    return s - str;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }
    return 0;
}

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *dest, int c, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = c;
    }

    return dest;
}
#endif