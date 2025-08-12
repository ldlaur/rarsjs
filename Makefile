AFL_CC ?= afl-clang-fast
LIBFUZZER_CC ?= clang
CFLAGS ?= -g
RARSJS_FLAGS ?= -Isrc/exec/ezld/include -g3
LIBFUZZER_FLAGS ?= $(RARSJS_FLAGS) -fsanitize=address -fsanitize=fuzzer
AFL_FLAGS ?= $(RARSJS_FLAGS) -O2 -fsanitize=address

EXEC_SRC = src/exec/core.c src/exec/emulate.c src/exec/callsan.c
SRC = $(EXEC_SRC) src/exec/vendor/commander.c src/exec/cli.c src/exec/elf.c
AFLSRC = $(EXEC_SRC) src/exec/afl.c
FUZZER_SRC = $(EXEC_SRC) src/exec/libfuzzer.c
TEST_SRC = $(EXEC_SRC) src/test/test.c src/unity/src/unity.c  
LIBEZLD = src/exec/ezld/bin/libezld.a

rarsjs: $(SRC) $(LIBEZLD)
	$(CC) $(CFLAGS) $(RARSJS_FLAGS) $(SRC) $(LIBEZLD) -o rarsjs

rarsjs_afl: $(AFLSRC) $(LIBEZLD)
	$(AFL_CC) $(CFLAGS) $(AFL_FLAGS) $(AFLSRC) $(LIBEZLD) -o rarsjs_afl

rarsjs_libfuzzer: $(FUZZER_SRC) $(LIBEZLD)
	$(LIBFUZZER_CC) $(CFLAGS) $(LIBFUZZER_FLAGS) $(LIBEZLD) $(FUZZER_SRC) -o rarsjs_libfuzzer

src/test/test_main.c: $(TEST_SRC)
	./src/test/gen_main.sh src/test/test.c > src/test/test_main.c

rarsjs_test: $(TEST_SRC) src/test/test_main.c $(LIBEZLD)
	clang $(CFLAGS) $(RARSJS_FLAGS) $(TEST_SRC) src/test/test_main.c $(LIBEZLD) -o rarsjs_test -Isrc/unity/src

rarsjs_test_cov: $(TEST_SRC) src/test/test_main.c $(LIBEZLD)
	clang $(CFLAGS) $(RARSJS_FLAGS) $(TEST_SRC) src/test/test_main.c $(LIBEZLD) -fprofile-instr-generate -fcoverage-mapping -o rarsjs_test -Isrc/unity/src

test_coverage: rarsjs_test_cov
	LLVM_PROFILE_FILE="rarsjs_test.profraw" ./rarsjs_test
	llvm-profdata merge -output=rarsjs_test.profdata rarsjs_test.profraw
	llvm-cov export --format=lcov ./rarsjs_test -instr-profile=rarsjs_test.profdata > lcov.info

$(LIBEZLD):
	cd src/exec/ezld && make library

clean:
	rm -f rarsjs rarsjs_afl rarsjs_libfuzzer
	cd src/exec/ezld && make clean

.PHONY: clean test_coverage
