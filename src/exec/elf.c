#include "rarsjs/elf.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rarsjs/core.h"

static LabelData *resolve_symbol(const char *sym, size_t sym_len, bool global) {
    LabelData *ret = NULL;

    for (size_t i = 0; i < g_labels_len; i++) {
        LabelData *l = &g_labels[i];

        if (0 == strncmp(sym, l->txt, sym_len)) {
            ret = l;
            break;
        }
    }

    return ret;
}

bool elf_read(const char *elf_path) {
    FILE *elf = fopen(elf_path, "rb");

    if (NULL == elf) {
        fprintf(stderr, "readelf: could not open file '%s'\n", elf_path);
        return false;
    }

    fseek(elf, 0, SEEK_END);
    size_t elf_sz = ftell(elf);
    rewind(elf);

    if (elf_sz < sizeof(ElfHeader)) {
        fprintf(stderr, "readelf: corrupt or invalid elf header\n");
        return false;
    }

    ElfHeader e_header = {0};
    fread(&e_header, sizeof(ElfHeader), 1, elf);

    if (0x7F != e_header.magic[0] || 'E' != e_header.magic[1] || 'L' != e_header.magic[2] || 'F' != e_header.magic[3]) {
        fprintf(stderr, "readelf: not an elf file\n");
        return false;
    }

    // Print magic bytes
    u8 *raw_header = (u8 *)&e_header;
    printf("Magic:");
    for (size_t i = 0; i < 8; i++) {
        printf(" %02x", raw_header[i]);
    }
    printf("\n");

    // Print file class
    printf("Class: ");
    if (1 == e_header.bits) {
        puts("ELF32");
    } else if (2 == e_header.bits) {
        puts("ELF64");
    } else {
        puts("Unknown");
    }

    // Print file endianness
    printf("Endianness: ");
    if (1 == e_header.endianness) {
        puts("Little endian");
    } else if (2 == e_header.endianness) {
        puts("Big endian");
    } else {
        puts("Unknown");
    }

    // Print ELF version
    printf("ELF version: %u\n", e_header.ehdr_ver);

    // Print OS/ABI
    printf("OS/ABI: ");
    if (0 == e_header.abi) {
        puts("UNIX - System V");
    } else {
        puts("Unknown");
    }

    // Print ELF type
    printf("ELF type: ");
    if (1 == e_header.type) {
        puts("Relocatable");
    } else if (2 == e_header.type) {
        puts("Executable");
    } else if (3 == e_header.type) {
        puts("Shared");
    } else if (4 == e_header.type) {
        puts("Core");
    } else {
        puts("Unknown");
    }

    // Print architecture
    printf("Architecture: ");
    if (0xF3 == e_header.isa) {
        puts("RISC-V");
    } else if (0x3E == e_header.isa) {
        puts("x86-64");
    } else if (0xB7 == e_header.isa) {
        puts("AArch64");
    } else {
        puts("Unknown");
    }

    // Print entry point address
    printf("Entry point virtual address: 0x%08x\n", e_header.entry);

    return true;
}

bool elf_emit_exec(const char *path) {
    FILE *out = fopen(path, "wb");

    if (NULL == out) {
        fprintf(stderr, "ERROR: could not open file '%s'\n", path);
        return false;
    }

    // LAYOUT
    // ELF header
    // Program headers (PHs)
    // Section headers (SHs)
    // Text PH
    // Data PH
    // String table

    const char str_tab[] = "\0.text\0.data\0str_tab\0";
    u32 segments_count = (0 != g_text_len) + (0 != g_data_len);
    u32 sections_count = 1 + segments_count;
    u32 phdrs_sz = sizeof(ElfProgramHeader) * segments_count;
    u32 shdrs_off = sizeof(ElfHeader) + phdrs_sz;
    u32 shdrs_sz = sizeof(ElfSectionHeader) * sections_count;
    u32 text_seg_off = shdrs_off + shdrs_sz;
    u32 data_seg_off = text_seg_off + g_text_len;
    u32 str_tab_off = data_seg_off + g_data_len;
    LabelData *start = resolve_symbol("_start", strlen("_start"), true);

    if (NULL == start) {
        fprintf(stderr, "linker: could not find `_start`\n");
        return false;
    }

    // Write ELF header
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

    fwrite(&e_hdr, sizeof(e_hdr), 1, out);

    // Write Text PH
    if (0 != g_text_len) {
        ElfProgramHeader text_header = {.type = PT_LOAD,
                                        .flags = 0b101,
                                        .off = text_seg_off,
                                        .virt_addr = TEXT_BASE,
                                        .phys_addr = TEXT_BASE,
                                        .file_sz = g_text_len,
                                        .mem_sz = g_text_len,
                                        .align = 4};
        fwrite(&text_header, sizeof(text_header), 1, out);
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
        fwrite(&data_header, sizeof(data_header), 1, out);
    }

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
        fwrite(&text_sec, sizeof(text_sec), 1, out);
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
        fwrite(&data_sec, sizeof(data_sec), 1, out);
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
    fwrite(&str_tab_sec, sizeof(str_tab_sec), 1, out);

    // Write text segment
    if (0 != g_text_len) {
        fwrite(g_text, g_text_len, 1, out);
    }

    // Write data section
    if (0 != g_data_len) {
        fwrite(g_data, g_data_len, 1, out);
    }

    // Write string table
    fwrite(str_tab, sizeof(str_tab), 1, out);

    fclose(out);
    return true;
}
