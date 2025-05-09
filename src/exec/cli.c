#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ezld/include/ezld/linker.h"
#include "ezld/include/ezld/runtime.h"
#include "rarsjs/core.h"
#include "rarsjs/elf.h"
#include "vendor/commander.h"

#define CHK_CMD()                                         \
    if (NULL != g_command) {                              \
        fprintf(stderr, "only one command is allowed\n"); \
        exit(-1);                                         \
    }                                                     \
    if (NULL != self->arg) {                              \
        g_next_arg = malloc(strlen(self->arg) + 1);       \
        strcpy(g_next_arg, self->arg);                    \
    }

// Type of command handler functions (c_*)
typedef void (*cmd_func_t)(void);

// Default output paths, changed by opt_o
static char *g_obj_out = "a.o";
static char *g_exec_out = "a.out";
static bool g_out_changed = false;

// Copies of argc, argv from main
static char **g_argv;
static int g_argc;

// The next argument in the command, set by opt_*
// for commands c_*
static char *g_next_arg = NULL;
static cmd_func_t g_command = NULL;

// These are non-command arguments
// Set in main
static const char **g_cmd_args;
static int g_cmd_args_len;

// UTILITY FUNCTIONS

static void emulate_safe(void) {
    while (!g_exited) {
        emulate();

        switch (g_runtime_error_type) {
            case ERROR_NONE:
                break;

            case ERROR_FETCH:
                fprintf(stderr,
                        "emulator: fetch error at pc=0x%08x on addr=0x%08x\n",
                        g_pc, g_runtime_error_params[0]);
                return;

            case ERROR_LOAD:
                fprintf(stderr,
                        "emulator: load error at pc=0x%08x on addr=0x%08x\n",
                        g_pc, g_runtime_error_params[0]);
                return;

            case ERROR_STORE:
                fprintf(stderr,
                        "emulator: store error at pc=0x%08x on addr=0x%08x\n",
                        g_pc, g_runtime_error_params[0]);
                return;

            case ERROR_UNHANDLED_INSN:
                fprintf(stderr, "emulator: unhandled insn at pc=0x%08x\n",
                        g_pc);
                return;

            default:
            fprintf(stderr, "emulator: unhandled error at pc=0x%08x\n",
                g_pc);

                return;
        }
    }
}

static void assemble_from_file(const char *src_path, bool allow_externs) {
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
    assemble(txt, s, allow_externs);

    if (g_error) {
        fprintf(stderr, "assembler: line %u %s\n", g_error_line, g_error);
    }
}

// COMMANDS

static void c_build(void) {
    assemble_from_file(g_next_arg, false);

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

    if (NULL == out) {
        fprintf(stderr, "linker: could not open output file\n");
        return;
    }

    fwrite(elf_contents, elf_sz, 1, out);
    fclose(out);
}

static void c_run(void) {
    FILE *elf = fopen(g_next_arg, "rb");

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

static void c_emulate(void) {
    assemble_from_file(g_next_arg, false);
    if (g_error) {
        return;
    }

    uint32_t addr;
    g_pc = TEXT_BASE;
    if (resolve_symbol("_start", strlen("_start"), true, &addr, NULL)) {
        g_pc = addr;
    }

    emulate_safe();
}

static void c_readelf(void) {
    FILE *elf = fopen(g_next_arg, "rb");

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
    printf(" %-35s: %u (bytes into file)\n", "Start of program headers",
           readelf.ehdr->phdrs_off);
    printf(" %-35s: %u (bytes into file)\n", "Start of section headers",
           readelf.ehdr->shdrs_off);
    printf(" %-35s: 0x%x\n", "Flags", readelf.ehdr->flags);
    printf(" %-35s: %u (bytes)\n", "Size of ELF header", readelf.ehdr->ehdr_sz);
    printf(" %-35s: %u (bytes)\n", "Size of each program header",
           readelf.ehdr->phent_sz);
    printf(" %-35s: %u\n", "Number of program headers",
           readelf.ehdr->phent_num);
    printf(" %-35s: %u (bytes)\n", "Size of each section header",
           readelf.ehdr->shent_sz);
    printf(" %-35s: %u\n", "Number of section headers",
           readelf.ehdr->shent_num);
    printf(" %-35s: %u\n", "Section header string table index",
           readelf.ehdr->shdr_str_idx);
    printf("\n");

    printf("Section headers:\n");
    printf(" [Nr] %-17s %-15s %-10s %-10s %-10s %-5s %-5s\n", "Name", "Type",
           "Address", "Offset", "Size", "Flags", "Align");

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
    printf(" %-14s %-10s %-15s %-16s %-10s %-5s %-5s\n", "Type", "Offset",
           "Virtual Address", "Physical Address", "Size", "Flags", "Align");
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

static void c_assemble(void) {
    assemble_from_file(g_next_arg, true);
    if (g_error) {
        return;
    }

    void *elf_contents = NULL;
    size_t elf_sz = 0;
    char *error = NULL;

    if (!elf_emit_obj(&elf_contents, &elf_sz, &error)) {
        fprintf(stderr, "assembler: %s\n", error);
        return;
    }

    FILE *out = fopen(g_obj_out, "wb");

    if (NULL == out) {
        fprintf(stderr, "assembelr: could not open output file\n");
        return;
    }

    fwrite(elf_contents, elf_sz, 1, out);
    fclose(out);
}

static void c_link(void) {
    const char *fake_argv[] = {"linker", NULL};
    ezld_runtime_init(1, fake_argv);
    ezld_config_t cfg = {0};

    cfg.entry_label = "_start";
    cfg.out_path = g_exec_out;
    cfg.seg_align = 0x1000;
    ezld_array_init(cfg.o_files);
    ezld_array_init(cfg.sections);
    *ezld_array_push(cfg.sections) =
        (ezld_sec_cfg_t){.name = ".text", .virt_addr = TEXT_BASE};
    *ezld_array_push(cfg.sections) =
        (ezld_sec_cfg_t){.name = ".data", .virt_addr = DATA_BASE};

    for (int i = 0; i < g_cmd_args_len; i++) {
        const char *filpath = g_cmd_args[i];
        *ezld_array_push(cfg.o_files) = filpath;
    }

    ezld_link(cfg);
    ezld_array_free(cfg.o_files);
    ezld_array_free(cfg.sections);
}

static void c_hexdump() {
    FILE *file = fopen(g_next_arg, "rb");

    if (NULL == file) {
        fprintf(stderr, "hexdump: could not open file\n");
        return;
    }

    u8 bytes[16];
    size_t bytes_read = 0;
    u32 off = 0;
    printf("[ Offset ]    %8s %8s %8s %8s\n", "[0 - 3]", "[4 - 7]", "[8 - 11]",
           "[12 - 15]");
    while ((bytes_read = fread(bytes, 1, 16, file))) {
        printf("[%08x]    ", off);
        for (size_t i = 0; i < bytes_read; i += 4) {
            for (size_t j = 0; j < 4 && i + j < bytes_read; j++) {
                printf("%02x", bytes[i + j]);
            }
            printf(" ");
        }
        printf("\n");
        off += bytes_read;
    }
}

static void c_ascii(void) {
    FILE *file = fopen(g_next_arg, "rb");

    if (NULL == file) {
        fprintf(stderr, "ascii: could not open file\n");
        return;
    }

    char bytes[16];
    size_t bytes_read = 0;
    u32 off = 0;
    printf(
        "[ Offset ]    +00 +01 +02 +03 +04 +05 +06 +07 +08 +09 +10 +11 +12 "
        "+13 +14 +15\n");

    while ((bytes_read = fread(bytes, 1, 16, file))) {
        printf("[%08x]    ", off);
        for (size_t i = 0; i < bytes_read; i++) {
            unsigned char c = bytes[i];

            printf(" ");
            switch (c) {
                case 0:
                    printf("\\0");
                    break;

                case '\n':
                    printf("\\n");
                    break;

                case '\r':
                    printf("\\r");
                    break;

                case '\t':
                    printf("\\t");
                    break;

                case '\a':
                    printf("\\a");
                    break;

                case '\b':
                    printf("\\b");
                    break;

                default:
                    if (c >= 32 && c < 127) {
                        printf(" %c", c);
                    } else {
                        printf("%02x", c);
                    }
                    break;
            }
            printf(" ");
        }
        printf("\n");
        off += bytes_read;
    }
}

// OPTIONS

static void opt_assemble(command_t *self) {
    CHK_CMD();
    g_command = c_assemble;
}

static void opt_build(command_t *self) {
    CHK_CMD();
    g_command = c_build;
}

static void opt_run(command_t *self) {
    CHK_CMD();
    g_command = c_run;
}

static void opt_emulate(command_t *self) {
    CHK_CMD();
    g_command = c_emulate;
}

static void opt_readelf(command_t *self) {
    CHK_CMD();
    g_command = c_readelf;
}

static void opt_o(command_t *self) {
    g_obj_out = malloc(strlen(self->arg) + 1);
    strcpy(g_obj_out, self->arg);
    g_exec_out = g_obj_out;
    g_out_changed = true;
}

static void opt_hexdump(command_t *self) {
    CHK_CMD();
    g_command = c_hexdump;
}

static void opt_link(command_t *self) {
    CHK_CMD();
    g_command = c_link;
}

static void opt_ascii(command_t *self) {
    CHK_CMD();
    g_command = c_ascii;
}

int main(int argc, char **argv) {
    atexit(free_runtime);
    g_argc = argc;
    g_argv = argv;

    command_t cmd;
    // TODO: place real version number
    command_init(&cmd, argv[0], "0.0.1");

    command_option(&cmd, "-a", "--assemble <file>",
                   "assemble an RV32 assembly file and output an ELF32 "
                   "relocatable object file",
                   opt_assemble);
    command_option(&cmd, "-b", "--build <file>",
                   "assemble an RV32 assembly file"
                   " and output an ELF32 executable",
                   opt_build);
    command_option(&cmd, "-r", "--run <file>", "run an ELF32 executable",
                   opt_run);
    command_option(&cmd, "-e", "--emulate <file>",
                   "assemble and run an RV32 assembly file", opt_emulate);
    command_option(&cmd, "-i", "--readelf <file>",
                   "show information about ELF file", opt_readelf);
    command_option(&cmd, "-x", "--hexdump <file>", "perform hexdump of file",
                   opt_hexdump);
    command_option(&cmd, "-c", "--ascii <file>", "perform ascii dump of file",
                   opt_ascii);
    command_option(&cmd, "-l", "--link", "link object files using ezld linker",
                   opt_link);
    command_option(&cmd, "-o", "--output <file>", "choose output file name",
                   opt_o);
    command_parse(&cmd, argc, argv);
    g_cmd_args = (const char **)cmd.argv;
    g_cmd_args_len = cmd.argc;

    if (1 == argc || NULL == g_command) {
        command_help(&cmd);
        command_free(&cmd);
        return EXIT_FAILURE;
    }

    g_command();
    free((void *)g_next_arg);
    if (g_out_changed) {
        free((void *)g_obj_out);
    }
    command_free(&cmd);
    return EXIT_SUCCESS;
}
