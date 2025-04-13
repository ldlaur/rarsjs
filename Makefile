rarsjs:
	clang -fsanitize=address src/exec/core.c src/exec/emulate.c src/exec/vendor/commander.c src/exec/cli.c src/exec/elf.c -g -o rarsjs
rarsjs_afl:
	afl-clang-fast -O2 -fsanitize=memory src/exec/core.c src/exec/emulate.c src/exec/afl.c -g -o rarsjs_afl
rarsjs_libfuzzer:
	clang -O3 -fsanitize=address -fsanitize=fuzzer src/exec/core.c src/exec/emulate.c src/exec/libfuzzer.c -g -o rarsjs_libfuzzer
