#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "rarsjs/core.h"
#include "rarsjs/elf.h"
#include "vendor/commander.h"

void assemble_from_file(const char *src_path) {
    FILE *f = fopen(src_path, "r");

    fseek(f, 0, SEEK_END);
    size_t s = ftell(f);
    rewind(f);
    char *txt = malloc(s);
    fread(txt, s, 1, f);
    assemble(txt, s);
}

// CLI commands
static void c_build(command_t *self) {
    assemble_from_file(self->arg);
    elf_emit_exec("a.elf");
}

static void c_run(command_t *self) { fprintf(stderr, "ERROR: Not implemented yet\n"); }
static void c_emulate(command_t *self) {
    assemble_from_file(self->arg);

    while (g_pc < TEXT_BASE + g_text_len) {
        emulate();
    }
}

static void c_readelf(command_t *self) { elf_read(self->arg); }

int main(int argc, char **argv) {
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
    command_parse(&cmd, argc, argv);

    if (1 == argc) {
        command_help(&cmd);
    }
}
