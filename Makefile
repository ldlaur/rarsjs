AFL_CC ?= afl-clang-fast
LIBFUZZER_CC ?= clang
CFLAGS ?= -g
LIBFUZZER_FLAGS ?= -fsanitize=address -fsanitize=fuzzer
AFL_FLAGS ?= -O2 -fsanitize=memory

SRC = src/exec/core.c src/exec/emulate.c src/exec/vendor/commander.c src/exec/cli.c src/exec/elf.c
AFLSRC = src/exec/core.c src/exec/emulate.c src/exec/afl.c
FUZZER_SRC = src/exec/core.c src/exec/emulate.c src/exec/libfuzzer.c
LIBEZLD = src/exec/ezld/bin/libezld.a

rarsjs: $(SRC) $(LIBEZLD)
	$(CC) $(SRC) $(LIBEZLD) $(CFLAGS) -o rarsjs

rarsjs_afl: $(AFLSRC) $(LIBEZLD)
	$(AFL_CC) $(AFL_FLAGS) $(AFLSRC) $(LIBEZLD) $(CFLAGS) -o rarsjs_afl

rarsjs_libfuzzer: $(FUZZER_SRC) $(LIBEZLD)
	$(LIBFUZZER_CC) $(LIBFUZZER_FLAGS) $(LIBEZLD) $(FUZZER_SRC) $(CFLAGS) -o rarsjs_libfuzzer

$(LIBEZLD):
	cd src/exec/ezld && make library

clean:
	rm -f rarsjs rarsjs_afl rarsjs_libfuzzer
	cd src/exec/ezld && make clean

.PHONY: clean
