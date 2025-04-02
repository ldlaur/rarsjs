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
    // Program headers
    // NULL section header
    // Sections headers
    // RARSJS_STRINGS section header
    // Segments
    // RARSJS_STRINGS (string table)

    u32 segments_count = 0;
    size_t segments_sz = 0;
    size_t str_tab_sz = 1 + strlen("RARSJS_STRINGS") + 1;
    size_t str_tab_idx = 1;

    for (size_t i = 0; i < g_sections_len; i++) {
        Section *s = g_sections[i];
        if (s->physical && 0 != s->len) {
            segments_count++;
            segments_sz += s->len;
            str_tab_sz += strlen(s->name) + 1;
        }
    }

    // Generate string table
    char *str_tab = malloc(str_tab_sz);
    CHK_OOM(str_tab, error);
    memset(str_tab, 0, str_tab_sz);
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *s = g_sections[i];
        if (s->physical && 0 != s->len) {
            WRITE_BUF(str_tab, s->name, strlen(s->name), &str_tab_idx);
            char zero = '\0';
            WRITE(str_tab, &zero, &str_tab_idx);
        }
    }
    WRITE_BUF(str_tab, "RARSJS_STRINGS", strlen("RARSJS_STRINGS"), &str_tab_idx);
    str_tab[str_tab_idx] = '\0';

    u32 sections_count = 2 + segments_count;
    u32 phdrs_sz = sizeof(ElfProgramHeader) * segments_count;
    u32 shdrs_off = sizeof(ElfHeader) + phdrs_sz;
    u32 shdrs_sz = sizeof(ElfSectionHeader) * sections_count;
    u32 segments_off = shdrs_off + shdrs_sz;
    u32 str_tab_off = segments_off + segments_sz;
    LabelData *start = resolve_symbol("_start", strlen("_start"), true);
    u8 *elf_contents = NULL;
    u32 elf_off = 0;

    if (NULL == start) {
        *error = "unresolved reference to `_start`";
        return false;
    }

    elf_contents = malloc(segments_sz + sizeof(ElfHeader) + phdrs_sz + shdrs_sz + str_tab_sz);
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

    // Offset where the i-th segment will be palced
    // starting at `segments_off` (start of segments)
    u32 segment_off = segments_off;

    // Write program headers
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *s = g_sections[i];
        if (s->physical && 0 != s->len) {
            // Ugly but avoids UB
            u32 flags = 0;
            if (s->read) {
                flags |= 0b100;
            }
            if (s->write) {
                flags |= 0b010;
            }
            if (s->execute) {
                flags |= 0b001;
            }

            ElfProgramHeader prog_header = {.type = PT_LOAD,
                                            .flags = flags,
                                            .off = segment_off,
                                            .virt_addr = s->base,
                                            .phys_addr = s->base,
                                            .file_sz = s->len,
                                            .mem_sz = s->len,
                                            .align = s->align};
            WRITE(elf_contents, &prog_header, &elf_off);
            segment_off += s->len;
        }
    }

    // Write NULL SH
    ElfSectionHeader null = {0};
    null.type = SHT_NULL;
    WRITE(elf_contents, &null, &elf_off);

    // Reset the segment offset to the base
    // This is done to set the stage for writing section headers
    // in the same order in which they appear in the program headers
    segment_off = segments_off;

    // Offset into the string table where the section name can be found
    size_t sec_name_off = 1;

    // Write section headers
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *s = g_sections[i];
        if (s->physical && 0 != s->len) {
            // Ugly but avoids UB
            u32 flags = SHF_ALLOC;
            if (s->write) {
                flags |= SHF_WRITE;
            }
            if (s->execute) {
                flags |= SHF_EXECINSTR;
            }

            ElfSectionHeader sec_header = {.name_off = sec_name_off,
                                           .type = SHT_PROGBITS,
                                           .flags = flags,
                                           .off = segment_off,
                                           .virt_addr = s->base,
                                           .mem_sz = s->len,
                                           .align = s->align,
                                           .link = 0,
                                           .ent_sz = 0};
            WRITE(elf_contents, &sec_header, &elf_off);
            segment_off += s->len;
            sec_name_off += strlen(s->name) + 1;
        }
    }

    // Write string table SH
    ElfSectionHeader str_tab_sec = {.name_off = sec_name_off,  // Should point to RARSJS_STRINGS\0
                                    .type = SHT_STRTAB,
                                    .flags = SHF_STRINGS,
                                    .off = str_tab_off,
                                    .virt_addr = 0,
                                    .mem_sz = str_tab_sz,
                                    .align = 1,
                                    .link = 0,
                                    .ent_sz = 0};
    WRITE(elf_contents, &str_tab_sec, &elf_off);

    // Write the segments themselves
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *s = g_sections[i];
        if (s->physical && 0 != s->len) {
            WRITE_BUF(elf_contents, s->buf, s->len, &elf_off);
        }
    }

    // Write the string table
    WRITE_BUF(elf_contents, str_tab, str_tab_sz, &elf_off);

    *out = elf_contents;
    *len = elf_off;
    return true;
fail:
    CLEANUP(elf_contents);
    CLEANUP(str_tab);
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

    for (u32 i = 0; i < e_header->shent_num; i++) {
        ElfSectionHeader *s_hdr = &shdrs[i];
        if (!(SHF_ALLOC & s_hdr->flags)) {
            continue;
        }

        Section *s = malloc(sizeof(Section));
        CHK_OOM(s, error);
        s->read = true;
        s->align = s_hdr->align;
        s->base = s_hdr->virt_addr;
        s->len = s_hdr->mem_sz;
        s->limit = s->base + s->len;
        s->buf = elf_contents + s_hdr->off;

        if (s_hdr->name_off >= str_tab_len) {
            *error = "section header name offset out of range";
            goto fail;
        }

        s->name = str_tab + s_hdr->name_off;

        if (SHF_WRITE & s_hdr->flags) {
            s->write = true;
        }

        if (SHF_EXECINSTR & s_hdr->flags) {
            s->execute = true;
        }

        *push(g_sections, g_sections_len, g_sections_cap) = s;
    }

    g_pc = e_header->entry;
    return true;

fail:
    for (size_t i = 0; i < g_sections_len; i++) {
        CLEANUP(g_sections[i]);
    }
    g_sections_len = 0;
    g_sections_cap = 0;
    CLEANUP(g_sections);
    return false;
}
