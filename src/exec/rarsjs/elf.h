#pragma once

#include <stdbool.h>

#include "types.h"

// Source from SalernOS Kernel: https://github.com/Alessandro-Salerno/SalernOS-Kernel/blob/main/src/com/sys/elf.c
// Docs from: https://wiki.osdev.org/ELF
typedef struct {
    u8 magic[4];
    u8 bits;
    u8 endianness;
    u8 ehdr_ver;
    u8 abi;
    u64 unused;
    u16 type;
    u16 isa;
    u32 elf_ver;
    u32 entry;
    u32 phdrs_off;
    u32 shdrs_off;
    u32 flags;
    u16 ehdr_sz;
    u16 phent_sz;
    u16 phent_num;
    u16 shent_sz;
    u16 shent_num;
    u16 shdr_str_idx;
} __attribute__((packed)) ElfHeader;

typedef struct {
    u32 type;
    u32 off;
    u32 virt_addr;
    u32 phys_addr;
    u32 file_sz;
    u32 mem_sz;
    u32 flags;
    u32 align;
} ElfProgramHeader;

typedef struct {
    u32 name_off;
    u32 type;
    u32 flags;
    u32 virt_addr;
    u32 off;
    u32 mem_sz;
    u32 link;
    u32 info;
    u32 align;
    u32 ent_sz;
} __attribute__((packed)) ElfSectionHeader;

bool elf_read(const char *elf_path);
bool elf_emit_exec(const char *path);
