#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rarsjs/core.h"
#include "rarsjs/elf.h"
#include "vendor/commander.h"

static const char *g_obj_out = "a.o";
static const char *g_exec_out = "a.out";

static void emulate_safe(void) {
    while (!g_exited) {
        emulate();

        switch (g_runtime_error_type) {
            case ERROR_FETCH:
                fprintf(stderr, "emulator: fetch error at pc=0x%08x on addr=0x%08x\n", g_pc, g_runtime_error_addr);
                return;

            case ERROR_LOAD:
                fprintf(stderr, "emulator: load error at pc=0x%08x on addr=0x%08x\n", g_pc, g_runtime_error_addr);
                return;

            case ERROR_STORE:
                fprintf(stderr, "emulator: store error at pc=0x%08x on addr=0x%08x\n", g_pc, g_runtime_error_addr);
                return;

            default:
                break;
        }
    }
}

static void assemble_from_file(const char *src_path) {
    FILE *f = fopen(src_path, "r");

    if (NULL == f) {
        g_error = "assembler: could not open input file";
        fprintf(stderr, "%s\n", g_error);
        return;
    }

    fseek(f, 0, SEEK_END);
    size_t s = ftell(f);
    rewind(f);
    char *txt = malloc(s);
    fread(txt, s, 1, f);
    assemble(txt, s);

    if (g_error) {
        fprintf(stderr, "assembler: line %u %s\n", g_error_line, g_error);
    }
}

static void c_build(command_t *self) {
    assemble_from_file(self->arg);

    if (g_error) {
        return;
    }

    void *elf_contents = NULL;
    size_t elf_sz = 0;
    char *error = NULL;

    if (!elf_emit_exec(&elf_contents, &elf_sz, &error)) {
        fprintf(stderr, "linker: %s\n", error);
        return;
    }

    FILE *out = fopen(g_exec_out, "wb");
    FILE *out2 = fopen("dump", "wb");

    if (NULL == out) {
        fprintf(stderr, "linker: could not open output file\n");
        return;
    }

    fwrite(elf_contents, elf_sz, 1, out);
    fwrite(g_text.buf, g_text.len, 1, out2);
}

static void c_run(command_t *self) {
    FILE *elf = fopen(self->arg, "rb");

    if (NULL == elf) {
        fprintf(stderr, "loader: could not open input file\n");
        return;
    }

    fseek(elf, 0, SEEK_END);
    size_t sz = ftell(elf);
    rewind(elf);

    u8 *elf_contents = malloc(sz);

    if (NULL == elf_contents) {
        fprintf(stderr, "loader: out of memory\n");
        return;
    }

    fread(elf_contents, sz, 1, elf);

    char *error = NULL;
    if (!elf_load(elf_contents, sz, &error)) {
        fprintf(stderr, "loader: %s\n", error);
        return;
    }

    emulate_safe();
}

static void c_emulate(command_t *self) {
    assemble_from_file(self->arg);
    if (g_error) {
        return;
    }

    uint32_t addr;
    g_pc = TEXT_BASE;
    if (resolve_symbol("_start", strlen("_start"), true, &addr)) {
        g_pc = addr;
    }

    emulate_safe();
}

static void c_readelf(command_t *self) {
    FILE *elf = fopen(self->arg, "rb");

    if (NULL == elf) {
        fprintf(stderr, "readelf: could not open input file\n");
        return;
    }

    fseek(elf, 0, SEEK_END);
    size_t sz = ftell(elf);
    rewind(elf);

    u8 *elf_contents = malloc(sz);

    if (NULL == elf_contents) {
        fprintf(stderr, "readelf: out of memory\n");
        return;
    }

    fread(elf_contents, sz, 1, elf);
    ReadElfResult readelf = {0};
    char *error = NULL;

    if (!elf_read(elf_contents, sz, &readelf, &error)) {
        fprintf(stderr, "readelf: %s\n", error);
        return;
    }

    printf(" %-35s:", "Magic");
    for (size_t i = 0; i < 8; i++) {
        printf(" %02x", readelf.magic8[i]);
    }
    printf("\n");

    printf(" %-35s: %s\n", "Class", readelf.class);
    printf(" %-35s: %s\n", "Endianness", readelf.endianness);
    printf(" %-35s: %u\n", "Version", readelf.ehdr->ehdr_ver);
    printf(" %-35s: %s\n", "OS/ABI", readelf.abi);
    printf(" %-35s: %s\n", "Type", readelf.type);
    printf(" %-35s: %s\n", "Architecture", readelf.architecture);
    printf(" %-35s: 0x%08x\n", "Entry point", readelf.ehdr->entry);
    printf(" %-35s: %u (bytes into file)\n", "Start of program headers", readelf.ehdr->phdrs_off);
    printf(" %-35s: %u (bytes into file)\n", "Start of section headers", readelf.ehdr->shdrs_off);
    printf(" %-35s: 0x%x\n", "Flags", readelf.ehdr->flags);
    printf(" %-35s: %u (bytes)\n", "Size of ELF header", readelf.ehdr->ehdr_sz);
    printf(" %-35s: %u (bytes)\n", "Size of each program header", readelf.ehdr->phent_sz);
    printf(" %-35s: %u\n", "Number of program headers", readelf.ehdr->phent_num);
    printf(" %-35s: %u (bytes)\n", "Size of each section header", readelf.ehdr->shent_sz);
    printf(" %-35s: %u\n", "Number of section headers", readelf.ehdr->shent_num);
    printf(" %-35s: %u\n", "Section header string table index", readelf.ehdr->shdr_str_idx);
    printf("\n");

    printf("Section headers:\n");
    printf(" [Nr] %-17s %-15s %-10s %-10s %-10s %-5s %-5s\n", "Name", "Type", "Address", "Offset", "Size", "Flags",
           "Align");

    for (u32 i = 0; i < readelf.ehdr->shent_num; i++) {
        ReadElfSection *sec = &readelf.shdrs[i];
        // clang-format off
        printf(" [%2u] %-17s %-15s 0x%08x 0x%08x 0x%08x %5s %5u\n",
                i, sec->name, sec->type, sec->shdr->virt_addr,
               sec->shdr->off, sec->shdr->mem_sz, sec->flags, sec->shdr->align);
        // clang-format on
    }
    printf("\n");

    printf("Program headers:\n");
    printf(" %-14s %-10s %-15s %-16s %-10s %-5s %-5s\n", "Type", "Offset", "Virtual Address", "Physical Address",
           "Size", "Flags", "Align");
    for (u32 i = 0; i < readelf.ehdr->phent_num; i++) {
        ReadElfSegment *seg = &readelf.phdrs[i];
        // clang-format off
        printf(" %-14s 0x%08x 0x%08x      0x%08x       0x%08x %5s %5u\n",
                seg->type, seg->phdr->off, seg->phdr->virt_addr, seg->phdr->phys_addr,
                seg->phdr->mem_sz, seg->flags, seg->phdr->align);
        // clang-format on
    }
    printf("\n");
}

static void c_output(command_t *self) {
    g_obj_out = self->arg;
    g_exec_out = self->arg;
}

int main(int argc, char **argv) {
    atexit(free_runtime);
    command_t cmd;
    // TODO: place real version number
    command_init(&cmd, argv[0], "0.0.1");
    command_option(&cmd, "-b", "--build <file>",
                   "assemble an RV32 assembly file"
                   " and output an ELF32 executable",
                   c_build);
    command_option(&cmd, "-r", "--run <file>", "run an ELF32 executable", c_run);
    command_option(&cmd, "-e", "--emulate <file>", "assemble and run an RV32 assembly file", c_emulate);
    command_option(&cmd, "-i", "--readelf <file>", "show information about ELF file", c_readelf);
    command_option(&cmd, "-o", "--output <file>", "choose output file name", c_output);
    command_parse(&cmd, argc, argv);

    if (1 == argc) {
        command_help(&cmd);
    }
}
