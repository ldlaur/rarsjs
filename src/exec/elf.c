#include "rarsjs/elf.h"

#include <bits/posix2_lim.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rarsjs/core.h"

#define UNKNOWN(prop) (prop) = "Unknown"
#define CHK_OOM(ptr, err)       \
    if (!(ptr)) {               \
        *err = "out of memory"; \
        goto fail;              \
    }

#define CLEANUP(ptr) \
    if ((ptr)) {     \
        free(ptr);   \
    }

#define WRITE(dst, ptr, off)                       \
    memcpy((dst) + *(off), (ptr), sizeof(*(ptr))); \
    *(off) += sizeof(*(ptr))

#define WRITE_BUF(dst, src, src_sz, off)     \
    memcpy((dst) + *(off), (src), (src_sz)); \
    *(off) += (src_sz)

static bool find_section(u32 *out_idx, char *str_tab, u32 str_tab_len, const char *section) {
    for (u32 i = 0; i < str_tab_len; i++) {
        if (0 == strcmp(&str_tab[i], section)) {
            *out_idx = i;
            return true;
        }
    }

    return false;
}

bool elf_read(u8 *elf_contents, size_t elf_contents_len, ReadElfResult *out, char **error) {
    if (elf_contents_len < sizeof(ElfHeader)) {
        *error = "corrupt or invalid elf header";
        return false;
    }

    ReadElfSegment *readable_phdrs = NULL;
    ElfHeader *e_header = (ElfHeader *)elf_contents;
    ReadElfSection *readable_shdrs = NULL;

    if (0x7F != e_header->magic[0] || 'E' != e_header->magic[1] || 'L' != e_header->magic[2] ||
        'F' != e_header->magic[3]) {
        *error = "not an elf file";
        return false;
    }

    out->ehdr = e_header;
    out->magic8 = elf_contents;

    // Print file class
    if (1 == e_header->bits) {
        out->class = "ELF32";
    } else if (2 == e_header->bits) {
        out->class = "ELF64 (WARNING: Corrupt content ahead, format not supported)";
    } else {
        UNKNOWN(out->class);
    }

    // Print file endianness
    if (1 == e_header->endianness) {
        out->endianness = "Little endian";
    } else if (2 == e_header->endianness) {
        out->endianness = "Big endian";
    } else {
        UNKNOWN(out->class);
    }

    // Print OS/ABI
    if (0 == e_header->abi) {
        out->abi = "UNIX - System V";
    } else {
        UNKNOWN(out->abi);
    }

    // Print ELF type
    if (1 == e_header->type) {
        out->type = "Relocatable";
    } else if (2 == e_header->type) {
        out->type = "Executable";
    } else if (3 == e_header->type) {
        out->type = "Shared";
    } else if (4 == e_header->type) {
        out->type = "Core";
    } else {
        UNKNOWN(out->type);
    }

    // Print architecture
    if (0xF3 == e_header->isa) {
        out->architecture = "RISC-V";
    } else if (0x3E == e_header->isa) {
        out->architecture = "x86-64 (x64, AMD/Intel 64 bit)";
    } else if (0xB7 == e_header->isa) {
        out->architecture = "AArch64 (ARM64)";
    } else {
        UNKNOWN(out->architecture);
    }

    ElfProgramHeader *phdrs = (ElfProgramHeader *)(elf_contents + e_header->phdrs_off);
    readable_phdrs = malloc(sizeof(ReadElfSegment) * e_header->phent_num);
    CHK_OOM(readable_phdrs, error);

    for (u32 i = 0; i < e_header->phent_num; i++) {
        ElfProgramHeader *phdr = &phdrs[i];
        ReadElfSegment *readable = &readable_phdrs[i];
        size_t flags_idx = 0;

        readable->phdr = phdr;

        if (0b100 & phdr->flags) {
            readable->flags[flags_idx++] = 'R';
        }

        if (0b010 & phdr->flags) {
            readable->flags[flags_idx++] = 'W';
        }

        if (0b001 & phdr->flags) {
            readable->flags[flags_idx++] = 'X';
        }

        readable->flags[flags_idx] = 0;

        switch (phdr->type) {
            case PT_LOAD:
                readable->type = "LOAD";
                break;

            case PT_NULL:
                readable->type = "NULL";
                break;

            case PT_DYNAMIC:
                readable->type = "DYNAMIC";
                break;

            case PT_INTERP:
                readable->type = "INTERP";
                break;

            case PT_NOTE:
                readable->type = "NOTE";
                break;

            default:
                UNKNOWN(readable->type);
                break;
        }
    }

    ElfSectionHeader *shdrs = (ElfSectionHeader *)(elf_contents + e_header->shdrs_off);
    readable_shdrs = malloc(sizeof(ReadElfSection) * e_header->shent_num);
    CHK_OOM(readable_shdrs, error);

    ElfSectionHeader *strtab = &shdrs[e_header->shdr_str_idx];
    char *strings = (char *)(elf_contents + strtab->off);

    for (u32 i = 0; i < e_header->shent_num; i++) {
        ElfSectionHeader *shdr = &shdrs[i];
        ReadElfSection *readable = &readable_shdrs[i];
        size_t flags_idx = 0;

        readable->shdr = shdr;

        if (SHF_WRITE & shdr->flags) {
            readable->flags[flags_idx++] = 'W';
        }

        if (SHF_ALLOC & shdr->flags) {
            readable->flags[flags_idx++] = 'A';
        }

        if (SHF_STRINGS & shdr->flags) {
            readable->flags[flags_idx++] = 'S';
        }

        readable->flags[flags_idx] = 0;
        readable->name = &strings[shdr->name_off];

        switch (shdr->type) {
            case SHT_NULL:
                readable->type = "NULL";
                break;

            case SHT_PROGBITS:
                readable->type = "PROGBITS";
                break;

            case SHT_SYMTAB:
                readable->type = "SYMTAB";
                break;

            case SHT_STRTAB:
                readable->type = "STRTAB";
                break;

            default:
                UNKNOWN(readable->type);
                break;
        }
    }

    out->phdrs = readable_phdrs;
    out->shdrs = readable_shdrs;
    return true;

fail:
    CLEANUP(readable_phdrs);
    CLEANUP(readable_shdrs);
    return false;
}

bool elf_emit_exec(void **out, size_t *len, char **error) {
    // LAYOUT
    // ELF header
    // .text program header
    // .data program header
    // NULL section header
    // .text section header
    // .data section header
    // RARSJS_STRINGS section header
    // .text segment
    // .data segment
    // RARSJS_STRINGS (string table)

    const char str_tab[] = "\0.text\0.data\0RARSJS_STRINGS\0";
    u32 segments_count = (0 != g_text_len) + (0 != g_data_len);
    u32 sections_count = 2 + segments_count;
    u32 phdrs_sz = sizeof(ElfProgramHeader) * segments_count;
    u32 shdrs_off = sizeof(ElfHeader) + phdrs_sz;
    u32 shdrs_sz = sizeof(ElfSectionHeader) * sections_count;
    u32 text_seg_off = shdrs_off + shdrs_sz;
    u32 data_seg_off = text_seg_off + g_text_len;
    u32 str_tab_off = data_seg_off + g_data_len;
    LabelData *start = resolve_symbol("_start", strlen("_start"), true);
    u8 *elf_contents = NULL;
    u32 elf_off = 0;

    if (NULL == start) {
        *error = "unresolved reference to `_start`";
        return false;
    }

    elf_contents = malloc(g_text_len + g_data_len + sizeof(ElfHeader) + phdrs_sz + shdrs_sz + sizeof(str_tab));
    CHK_OOM(elf_contents, error);

    ElfHeader e_hdr = {.magic = {0x7F, 'E', 'L', 'F'},        // ELF magic
                       .bits = 1,                             // 32 bits
                       .endianness = 1,                       // little endian
                       .ehdr_ver = 1,                         // ELF header version 1
                       .abi = 0,                              // System V ABI
                       .type = 2,                             // Executable
                       .isa = 0xF3,                           // Arch = RISC-V
                       .elf_ver = 1,                          // ELF version 1
                       .entry = start->addr,                  // Program entrypoint
                       .phdrs_off = sizeof(ElfHeader),        // Start offset of program header tabe
                       .phent_num = segments_count,           // 2 program headers
                       .phent_sz = sizeof(ElfProgramHeader),  // Size of each program header table entry
                       .shdrs_off = shdrs_off,                // Start offset of section header table
                       .shent_num = sections_count,           // 2 sections (.text, .data)
                       .shent_sz = sizeof(ElfSectionHeader),  // Size of each section header
                       .ehdr_sz = sizeof(ElfHeader),          // Size of the ELF ehader
                       .flags = 0,                            // Flags
                       .shdr_str_idx = sections_count - 1};

    WRITE(elf_contents, &e_hdr, &elf_off);

    if (0 != g_text_len) {
        ElfProgramHeader text_header = {.type = PT_LOAD,
                                        .flags = 0b101,
                                        .off = text_seg_off,
                                        .virt_addr = TEXT_BASE,
                                        .phys_addr = TEXT_BASE,
                                        .file_sz = g_text_len,
                                        .mem_sz = g_text_len,
                                        .align = 4};
        WRITE(elf_contents, &text_header, &elf_off);
    }

    // Write data PH
    if (0 != g_data_len) {
        ElfProgramHeader data_header = {.type = PT_LOAD,
                                        .flags = 0b110,
                                        .off = data_seg_off,
                                        .virt_addr = DATA_BASE,
                                        .phys_addr = DATA_BASE,
                                        .file_sz = g_data_len,
                                        .mem_sz = g_data_len,
                                        .align = 1};
        WRITE(elf_contents, &data_header, &elf_off);
    }

    // Write NULL SH
    ElfSectionHeader null = {0};
    null.type = SHT_NULL;
    WRITE(elf_contents, &null, &elf_off);

    // Write text SH
    if (0 != g_text_len) {
        ElfSectionHeader text_sec = {.name_off = 1,
                                     .type = SHT_PROGBITS,
                                     .flags = SHF_EXECINSTR | SHF_ALLOC,
                                     .off = text_seg_off,
                                     .virt_addr = TEXT_BASE,
                                     .mem_sz = g_text_len,
                                     .align = 4,
                                     .link = 0,
                                     .ent_sz = 0};
        WRITE(elf_contents, &text_sec, &elf_off);
    }

    // Write data SH
    if (0 != g_data_len) {
        ElfSectionHeader data_sec = {.name_off = 7,
                                     .type = SHT_PROGBITS,
                                     .flags = SHF_WRITE | SHF_ALLOC,
                                     .off = data_seg_off,
                                     .virt_addr = DATA_BASE,
                                     .mem_sz = g_data_len,
                                     .align = 1,
                                     .link = 0,
                                     .ent_sz = 0};
        WRITE(elf_contents, &data_sec, &elf_off);
    }

    // Write string table SH
    ElfSectionHeader str_tab_sec = {.name_off = 13,
                                    .type = SHT_STRTAB,
                                    .flags = SHF_STRINGS,
                                    .off = str_tab_off,
                                    .virt_addr = 0,
                                    .mem_sz = sizeof(str_tab),
                                    .align = 1,
                                    .link = 0,
                                    .ent_sz = 0};
    WRITE(elf_contents, &str_tab_sec, &elf_off);

    if (0 != g_text_len) {
        WRITE_BUF(elf_contents, g_text, g_text_len, &elf_off);
    }

    if (0 != g_data_len) {
        WRITE_BUF(elf_contents, g_data, g_data_len, &elf_off);
    }

    WRITE_BUF(elf_contents, str_tab, sizeof(str_tab), &elf_off);

    *out = elf_contents;
    *len = elf_off;
    return true;
fail:
    CLEANUP(elf_contents);
    return false;
}

bool elf_load(u8 *elf_contents, size_t elf_len, char **error) {
    if (elf_len < sizeof(ElfHeader)) {
        *error = "corrupt or invalid elf header";
        return false;
    }

    ElfHeader *e_header = (ElfHeader *)elf_contents;

    if (0x7F != e_header->magic[0] || 'E' != e_header->magic[1] || 'L' != e_header->magic[2] ||
        'F' != e_header->magic[3]) {
        *error = "not an elf file";
        return false;
    }

    if (1 != e_header->bits) {
        *error = "unsupported elf variant (only elf32 is supported)";
        return false;
    }

    if (0xF3 != e_header->isa) {
        *error = "unsupported architecture (only risc-v is supported)";
        return false;
    }

    if (2 != e_header->type) {
        *error = "not an elf executable";
        return false;
    }

    ElfProgramHeader *phdrs = (ElfProgramHeader *)(elf_contents + e_header->phdrs_off);
    ElfSectionHeader *shdrs = (ElfSectionHeader *)(elf_contents + e_header->shdrs_off);

    ElfSectionHeader *str_tab_shdr = &shdrs[e_header->shdr_str_idx];
    char *str_tab = (char *)(elf_contents + str_tab_shdr->off);
    u32 str_tab_len = str_tab_shdr->mem_sz;
    u32 text_section_name_off = 0;
    u32 data_section_name_off = 0;

    if (!find_section(&text_section_name_off, str_tab, str_tab_len, ".text")) {
        *error = "executable has no .text section in string table";
        return false;
    }

    if (!find_section(&data_section_name_off, str_tab, str_tab_len, ".data")) {
        *error = "executable has no .data section in string table";
        return false;
    }

    if (text_section_name_off == data_section_name_off) {
        *error = ".text and .data string table entries overlap";
        return false;
    }

    ElfSectionHeader *text_section = NULL;
    ElfSectionHeader *data_section = NULL;

    for (u32 i = 0; i < e_header->shent_num; i++) {
        ElfSectionHeader *sec = &shdrs[i];

        if (sec->name_off == text_section_name_off) {
            text_section = sec;
        } else if (sec->name_off == data_section_name_off) {
            data_section = sec;
        }
    }

    // TODO: overlap checks

    g_pc = e_header->entry;

    if (NULL != text_section) {
        g_text = elf_contents + text_section->off;
        g_text_len = text_section->mem_sz;
    }

    if (NULL != data_section) {
        g_data = elf_contents + data_section->off;
        g_data_len = data_section->mem_sz;
    }

    return true;
}
