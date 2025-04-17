AFL_CC ?= afl-clang-fast
LIBFUZZER_CC ?= clang
CFLAGS ?= -g
LIBFUZZER_FLAGS ?= -fsanitize=address -fsanitize=fuzzer
AFL_FLAGS ?= -O2 -fsanitize=memory

SRC = src/exec/core.c src/exec/emulate.c src/exec/vendor/commander.c src/exec/cli.c src/exec/elf.c src/exec/callsan.c
AFLSRC = src/exec/core.c src/exec/emulate.c src/exec/callsan.c src/exec/afl.c
FUZZER_SRC = src/exec/core.c src/exec/emulate.c src/exec/callsan.c src/exec/libfuzzer.c

rarsjs: $(SRC)
	$(CC) $(SRC) $(CFLAGS) -o rarsjs

rarsjs_afl: $(AFLSRC)
	$(AFL_CC) $(AFL_FLAGS) $(AFLSRC) $(CFLAGS) -o rarsjs_afl

rarsjs_libfuzzer: $(FUZZER_SRC)
	$(LIBFUZZER_CC) $(LIBFUZZER_FLAGS) $(FUZZER_SRC) $(CFLAGS) -o rarsjs_libfuzzer

clean:
	rm -f rarsjs rarsjs_afl rarsjs_libfuzzer

.PHONY: clean