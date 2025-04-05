#include "rarsjs/elf.h"

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

#define CHK_CALL(expr) \
    if (!(expr)) {     \
        goto fail;     \
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

#define WRITE_STR(dst, src, off)                      \
    memcpy((dst) + *(off), (src), strlen((src)) + 1); \
    *(off) += strlen((src)) + 1

bool elf_read(u8 *elf_contents, size_t elf_contents_len, ReadElfResult *out, char **error) {
    if (NULL == elf_contents) {
        *error = "null buffer";
        return false;
    }

    if (elf_contents_len < sizeof(ElfHeader)) {
        *error = "corrupt or invalid elf header";
        return false;
    }

    ReadElfSegment *readable_phdrs = NULL;
    ReadElfSection *readable_shdrs = NULL;
    ElfHeader *e_header = (ElfHeader *)elf_contents;

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

    if (e_header->phdrs_off >= elf_contents_len ||
        e_header->phdrs_off + (e_header->phent_sz * e_header->phent_num) >= elf_contents_len) {
        *error = "program headers offset exceeds buffer size";
        goto fail;
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

    if (e_header->shdrs_off >= elf_contents_len ||
        e_header->shdrs_off + (e_header->shent_sz * e_header->shent_num) >= elf_contents_len) {
        *error = "section headers offset exceeds buffer size";
        goto fail;
    }

    ElfSectionHeader *shdrs = (ElfSectionHeader *)(elf_contents + e_header->shdrs_off);
    readable_shdrs = malloc(sizeof(ReadElfSection) * e_header->shent_num);
    CHK_OOM(readable_shdrs, error);

    ElfSectionHeader *str_sh = &shdrs[e_header->shdr_str_idx];
    char *str_tab = (char *)(elf_contents + str_sh->off);
    u32 str_tab_sz = str_sh->mem_sz;

    for (u32 i = 0; i < e_header->shent_num; i++) {
        ElfSectionHeader *shdr = &shdrs[i];
        ReadElfSection *readable = &readable_shdrs[i];
        size_t flags_idx = 0;

        if (shdr->name_off >= str_tab_sz) {
            *error = "section name out of bounds of string table section";
            goto fail;
        }

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

        if (SHF_EXECINSTR & shdr->flags) {
            readable->flags[flags_idx++] = 'X';
        }

        readable->flags[flags_idx] = 0;
        readable->name = &str_tab[shdr->name_off];

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

// Constructs a buffer containing program headers, segments and section headers
// ORDER;
// - Program headers
// - Segments
// - Section headers
// ORDER OF SECTION HEADERS:
// - NULL section
// - Reserved sections
// - Segment-related sections (in the same order as the segments)
// - Relocation sections (in the same order as the segments)
// ADDITIONAL INFORMATION:
// This function assumes that section names are the first strings in the strtab.
// This function also assumes that name_off points to a value > 0.
// phdrs_start, shdrs_start, name_off are all byte offsets.
// This functions also assumes that the names of relocation sections follow those of the relative section
// withing the string table. E.g., .rela.text comes immediately after .text
// NOTE: This function changes elf.shidx in each physical section in g_sections with len > 0
static bool make_core(u8 **out, size_t *out_sz, size_t *name_off, size_t *phdrs_start, size_t *shdrs_start,
                      size_t *phnum, size_t *shnum, size_t *reloc_idx, size_t *reloc_num, size_t file_off,
                      size_t rsv_shdrs, size_t symtab_idx, bool use_phdrs, bool use_shdrs, char **error) {
    size_t segments_count = 0;
    size_t segments_sz = 0;
    size_t reloc_shdrs_num = 0;
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *s = g_sections[i];
        if (s->physical && 0 != s->len) {
            segments_count++;
            segments_sz += s->len;

            if (0 != s->relocations.len) {
                reloc_shdrs_num++;
            }
        }
    }

    size_t sections_count = 1 + segments_count + rsv_shdrs + reloc_shdrs_num;
    size_t region_sz = segments_sz;

    if (use_phdrs) {
        region_sz += segments_count * sizeof(ElfProgramHeader);
    }

    if (use_shdrs) {
        region_sz += sections_count * sizeof(ElfSectionHeader);
    }

    u8 *region = malloc(region_sz);
    CHK_OOM(region, error);

    size_t phdrs_off = 0;
    size_t segment_off = 0;

    if (use_phdrs) {
        segment_off += segments_count * sizeof(ElfProgramHeader);
    }

    size_t shdrs_off = segment_off + segments_sz;
    size_t shdrs_i = 0;

    // Create null section
    if (use_shdrs) {
        ElfSectionHeader null_s = {0};
        null_s.type = SHT_NULL;
        WRITE(region, &null_s, &shdrs_off);
        shdrs_i++;
    }

    // Move past reserved sections
    shdrs_off += rsv_shdrs * sizeof(ElfSectionHeader);
    shdrs_i += rsv_shdrs;

    size_t reloc_off = shdrs_off + segments_count * sizeof(ElfSectionHeader);
    size_t reloc_i = 1 + rsv_shdrs + segments_count;

    // Return already known values
    if (NULL != reloc_idx) {
        *reloc_idx = reloc_i;
    }
    if (NULL != reloc_num) {
        *reloc_num = reloc_shdrs_num;
    }
    *out = region;
    *out_sz = region_sz;
    *phdrs_start = 0;
    *shdrs_start = segments_sz;
    if (use_phdrs) {
        *shdrs_start += segments_count * sizeof(ElfProgramHeader);
    }
    *phnum = segments_count;
    *shnum = sections_count;

    // Write program headers, segments, and section headers
    // RELOCATION HEADERS EXCLUDED
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *s = g_sections[i];
        if (!s->physical || 0 == s->len) {
            continue;
        }

        // Ugly but avoids UB
        u32 phdr_flags = 0;
        if (s->read) {
            phdr_flags |= 0b100;
        }
        if (s->write) {
            phdr_flags |= 0b010;
        }
        if (s->execute) {
            phdr_flags |= 0b001;
        }

        ElfProgramHeader prog_header = {.type = PT_LOAD,
                                        .flags = phdr_flags,
                                        .off = segment_off + file_off,
                                        .virt_addr = s->base,
                                        .phys_addr = s->base,
                                        .file_sz = s->len,
                                        .mem_sz = s->len,
                                        .align = s->align};

        // Ugly but avoids UB
        u32 shdr_flags = SHF_ALLOC;
        if (s->write) {
            shdr_flags |= SHF_WRITE;
        }
        if (s->execute) {
            shdr_flags |= SHF_EXECINSTR;
        }

        ElfSectionHeader sec_header = {.name_off = *name_off,
                                       .type = SHT_PROGBITS,
                                       .flags = shdr_flags,
                                       .off = segment_off + file_off,
                                       .virt_addr = s->base,
                                       .mem_sz = s->len,
                                       .align = s->align,
                                       .link = 0,
                                       .ent_sz = 0};

        if (use_phdrs) {
            WRITE(region, &prog_header, &phdrs_off);
        }
        WRITE_BUF(region, s->buf, s->len, &segment_off);
        if (use_shdrs) {
            s->elf.shidx = shdrs_i;
            *(name_off) += strlen(s->name);
            WRITE(region, &sec_header, &shdrs_off);
        }

        // Write relocation section header
        if (use_shdrs && 0 != s->relocations.len) {
            ElfSectionHeader reloc_shdr = {.name_off = *name_off,
                                           .type = SHT_RELA,
                                           .flags = SHF_INFO_LINK,
                                           .info = shdrs_i,
                                           .off = 0,
                                           .virt_addr = 0,
                                           .mem_sz = 0,
                                           .align = 1,
                                           .link = symtab_idx,
                                           .ent_sz = sizeof(ElfRelaEntry)};
            WRITE(region, &reloc_shdr, &reloc_off);
            *(name_off) += strlen(".rela") + strlen(s->name);
        }

        // Tail update to index to avoid issue with relocation sections
        shdrs_i++;
    }

    return true;
fail:
    free(region);
    return false;
}

// This function makes an ELF string table
// The string table always starts with:
// \0.strtab\0.symtab\0
// Thus, the indices for .startab and .symtab are 1 and 9 respectively
// Section names start at index 17
// Then come, in this order, externs and globals (if included)
static bool make_strtab(char **out, size_t *out_sz, bool inc_externs, bool inc_globs, char **error) {
    size_t base_len = strlen(".strtab") + 1 + strlen(".symtab") + 1;
    size_t strtab_sz = 1 + base_len;
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *s = g_sections[i];
        if (s->physical && 0 != s->len) {
            strtab_sz += strlen(s->name) + 1;

            if (0 != s->relocations.len) {
                strtab_sz += strlen(".rela") + strlen(s->name) + 1;
            }
        }
    }
    if (inc_externs) {
        for (size_t i = 0; i < g_externs_len; i++) {
            Extern *e = &g_externs[i];
            strtab_sz += e->len + 1;
        }
    }
    if (inc_globs) {
        for (size_t i = 0; i < g_globals_len; i++) {
            Global *g = &g_globals[i];
            strtab_sz += g->len + 1;
        }
    }

    char *strtab = malloc(strtab_sz);
    CHK_OOM(strtab, error);

    strtab[0] = '\0';
    size_t strtab_off = 1;

    WRITE_STR(strtab, ".strtab", &strtab_off);
    WRITE_STR(strtab, ".symtab", &strtab_off);
    for (size_t i = 0; i < g_sections_len; i++) {
        Section *s = g_sections[i];
        if (s->physical && 0 != s->len) {
            WRITE_STR(strtab, s->name, &strtab_off);

            if (0 != s->relocations.len) {
                WRITE_STR(strtab, ".rela", &strtab_off);
                strtab_off--;
                WRITE_STR(strtab, s->name, &strtab_off);
            }
        }
    }

    if (inc_externs) {
        for (size_t i = 0; i < g_externs_len; i++) {
            Extern *e = &g_externs[i];
            WRITE_BUF(strtab, e->symbol, e->len, &strtab_off);
            strtab[strtab_off++] = '\0';
        }
    }

    if (inc_globs) {
        for (size_t i = 0; i < g_globals_len; i++) {
            Global *g = &g_globals[i];
            WRITE_BUF(strtab, g->str, g->len, &strtab_off);
            strtab[strtab_off++] = '\0';
        }
    }

    *out = strtab;
    *out_sz = strtab_sz;
    return true;

fail:
    free(strtab);
    return false;
}

static bool make_symtab(u8 **out, size_t *out_sz, size_t *ent_num, size_t name_off, char **error) {
    size_t symtab_sz = sizeof(ElfSymtabEntry) * (1 + g_externs_len + g_globals_len);
    ElfSymtabEntry *symtab = malloc(symtab_sz);
    CHK_OOM(symtab, error);

    *out = (u8 *)symtab;
    *out_sz = symtab_sz;

    ElfSymtabEntry null_e = {0};
    null_e.shent_idx = SHN_UNDEF;
    symtab[0] = null_e;

    size_t symtab_i = 1;

    for (size_t i = 0; i < g_externs_len; i++, symtab_i++) {
        Extern *e = &g_externs[i];
        ElfSymtabEntry *sym = &symtab[symtab_i];
        sym->name_off = name_off;
        sym->shent_idx = SHN_UNDEF;
        sym->other = 0;
        sym->size = 0;
        sym->value = 0;
        sym->info = ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE);
        name_off += e->len;
    }

    for (size_t i = 0; i < g_globals_len; i++, symtab_i++) {
        Global *g = &g_globals[i];
        ElfSymtabEntry *sym = &symtab[symtab_i];
        sym->name_off = name_off;
        sym->other = 0;
        sym->size = 0;

        u32 addr = 0;
        Section *sec = NULL;

        if (!resolve_symbol(g->str, g->len, true, &addr, &sec)) {
            *error = "symbol is declared global but never defined";
            goto fail;
        }

        sym->shent_idx = sec->elf.shidx;
        sym->value = addr - sec->base;
        sym->info = ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE);
        name_off += g->len;
    }

    *ent_num = symtab_i;
    return true;

fail:
    free(symtab);
    return false;
}

static bool make_rela(u8 **out, size_t *out_sz, char **error) {
fail:
    return false;
}

bool elf_emit_exec(void **out, size_t *len, char **error) {
    char *strtab = NULL;
    u8 *core = NULL;
    size_t strtab_sz = 0;
    size_t core_sz = 0;
    size_t name_off = 17;
    size_t phdrs_start = 0;
    size_t shdrs_start = 0;
    size_t phnum = 0;
    size_t shnum = 0;

    u32 entrypoint;
    if (!resolve_symbol("_start", strlen("_start"), true, &entrypoint, NULL)) {
        *error = "unresolved reference to `_start`";
        return false;
    }

    CHK_CALL(make_strtab(&strtab, &strtab_sz, true, true, error));
    CHK_CALL(make_core(&core, &core_sz, &name_off, &phdrs_start, &shdrs_start, &phnum, &shnum, NULL, NULL,
                       sizeof(ElfHeader), 1, 0, true, true, error));

    ElfHeader e_hdr = {.magic = {0x7F, 'E', 'L', 'F'},                // ELF magic
                       .bits = 1,                                     // 32 bits
                       .endianness = 1,                               // little endian
                       .ehdr_ver = 1,                                 // ELF header version 1
                       .abi = 0,                                      // System V ABI
                       .type = 2,                                     // Executable
                       .isa = 0xF3,                                   // Arch = RISC-V
                       .elf_ver = 1,                                  // ELF version 1
                       .entry = entrypoint,                           // Program entrypoint
                       .phdrs_off = sizeof(ElfHeader) + phdrs_start,  // Start offset of program header tabe
                       .phent_num = phnum,                            // 2 program headers
                       .phent_sz = sizeof(ElfProgramHeader),          // Size of each program header table entry
                       .shdrs_off = sizeof(ElfHeader) + shdrs_start,  // Start offset of section header table
                       .shent_num = shnum,                            // 2 sections (.text, .data)
                       .shent_sz = sizeof(ElfSectionHeader),          // Size of each section header
                       .ehdr_sz = sizeof(ElfHeader),                  // Size of the ELF ehader
                       .flags = 0,                                    // Flags
                       .shdr_str_idx = 1};

    ElfSectionHeader *shdrs = (ElfSectionHeader *)(core + shdrs_start);
    shdrs[1] = (ElfSectionHeader){.name_off = 1,
                                  .type = SHT_STRTAB,
                                  .flags = 0,
                                  .off = sizeof(ElfHeader) + core_sz,
                                  .virt_addr = 0,
                                  .mem_sz = strtab_sz,
                                  .align = 1,
                                  .link = 0,
                                  .ent_sz = 0};

    u8 *elf_contents = malloc(sizeof(ElfHeader) + core_sz + strtab_sz);
    CHK_OOM(elf_contents, error);
    size_t elf_off = 0;

    WRITE(elf_contents, &e_hdr, &elf_off);
    WRITE_BUF(elf_contents, core, core_sz, &elf_off);
    WRITE_BUF(elf_contents, strtab, strtab_sz, &elf_off);
    *out = elf_contents;
    *len = elf_off;

    free(core);
    free(strtab);
    return true;

fail:
    free(strtab);
    free(core);
    free(elf_contents);
    return false;
}

bool elf_emit_obj(void **out, size_t *len, char **error) {
    char *strtab = NULL;
    u8 *core = NULL;
    u8 *symtab = NULL;
    size_t strtab_sz = 0;
    size_t core_sz = 0;
    size_t name_off = 17;
    size_t phdrs_start = 0;
    size_t shdrs_start = 0;
    size_t phnum = 0;
    size_t shnum = 0;
    size_t reloc_idx = 0;
    size_t reloc_num = 0;
    size_t symtab_sz = 0;
    size_t symtab_entnum = 0;

    CHK_CALL(make_strtab(&strtab, &strtab_sz, true, true, error));
    CHK_CALL(make_core(&core, &core_sz, &name_off, &phdrs_start, &shdrs_start, &phnum, &shnum, &reloc_idx, &reloc_num,
                       sizeof(ElfHeader), 2, 0, false, true, error));
    CHK_CALL(make_symtab(&symtab, &symtab_sz, &symtab_entnum, name_off + 1, error));

    ElfHeader e_hdr = {.magic = {0x7F, 'E', 'L', 'F'},                // ELF magic
                       .bits = 1,                                     // 32 bits
                       .endianness = 1,                               // little endian
                       .ehdr_ver = 1,                                 // ELF header version 1
                       .abi = 0,                                      // System V ABI
                       .type = 1,                                     // Executable
                       .isa = 0xF3,                                   // Arch = RISC-V
                       .elf_ver = 1,                                  // ELF version 1
                       .entry = 0,                                    // Program entrypoint
                       .phdrs_off = 0,                                // Start offset of program header tabe
                       .phent_num = 0,                                // 2 program headers
                       .phent_sz = 0,                                 // Size of each program header table entry
                       .shdrs_off = sizeof(ElfHeader) + shdrs_start,  // Start offset of section header table
                       .shent_num = shnum,                            // 2 sections (.text, .data)
                       .shent_sz = sizeof(ElfSectionHeader),          // Size of each section header
                       .ehdr_sz = sizeof(ElfHeader),                  // Size of the ELF ehader
                       .flags = 0,                                    // Flags
                       .shdr_str_idx = 1};

    ElfSectionHeader *shdrs = (ElfSectionHeader *)(core + shdrs_start);
    shdrs[1] = (ElfSectionHeader){.name_off = 1,
                                  .type = SHT_STRTAB,
                                  .flags = 0,
                                  .off = sizeof(ElfHeader) + core_sz,
                                  .virt_addr = 0,
                                  .mem_sz = strtab_sz,
                                  .align = 1,
                                  .link = 0,
                                  .ent_sz = 0};
    shdrs[2] = (ElfSectionHeader){.name_off = 9,
                                  .type = SHT_SYMTAB,
                                  .flags = 0,
                                  .info = symtab_entnum,
                                  .off = sizeof(ElfHeader) + core_sz + strtab_sz,
                                  .virt_addr = 0,
                                  .mem_sz = symtab_sz,
                                  .align = 1,
                                  .link = 1,
                                  .ent_sz = sizeof(ElfSymtabEntry)};

    u8 *elf_contents = malloc(sizeof(ElfHeader) + core_sz + strtab_sz + symtab_sz);
    CHK_OOM(elf_contents, error);
    size_t elf_off = 0;

    WRITE(elf_contents, &e_hdr, &elf_off);
    WRITE_BUF(elf_contents, core, core_sz, &elf_off);
    WRITE_BUF(elf_contents, strtab, strtab_sz, &elf_off);
    WRITE_BUF(elf_contents, symtab, symtab_sz, &elf_off);

    for (size_t i = reloc_idx; i < reloc_idx + reloc_num; i++) {
        // TODO: Make rela sections
    }

    *out = elf_contents;
    *len = elf_off;

    free(core);
    free(strtab);
    free(symtab);
    return true;

fail:
    free(strtab);
    free(core);
    free(symtab);
    free(elf_contents);
    return false;
}

bool elf_load(u8 *elf_contents, size_t elf_len, char **error) {
    if (NULL == elf_contents) {
        *error = "null buffer";
        return false;
    }

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
    prepare_stack();
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
