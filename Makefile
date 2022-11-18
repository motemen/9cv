CFLAGS=-std=c11 -g -Werror -Wno-error=format -fprofile-instr-generate -fcoverage-mapping
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

# stage0: clang
# stage1: mocc on host machine generated by stage0 (clang)
# stage2: mocc on RISC-V machine generated by stage1 (mocc on host machine)
# stage3: mocc on RISC-V machine generated by stage2 (mocc on RISC-V machine)

all: mocc-stage1 mocc-stage2.riscv mocc-stage3.riscv

test: test-stage1 test-stage2 test-stage3

mocc-stage1: $(OBJS)
	@echo "# stage1: mocc on host machine generated by stage0 (clang)"
	$(CC) $(CFLAGS) -o mocc-stage1 $(OBJS) $(LDFLAGS)

test-stage1: mocc-stage1
	LLVM_PROFILE_FILE=1.profraw ./mocc-stage1 test/test.c > tmp.s
	riscv64-$(RISCV_HOST)-gcc -static tmp.s test/helper.c -o test.riscv
	prove -v -e ./riscvw ./test.riscv
	LLVM_PROFILE_FILE=2.profraw MOCC=./mocc-stage1 prove -v ./test.sh

coverage.html: test-stage1
	llvm-profdata merge -sparse *.profraw -o mocc.profdata
	llvm-cov show ./mocc-stage1 -instr-profile=mocc.profdata -format=html > $@
	rm -f mocc.profdata

preprocess:
	@rm -rf .self
	@mkdir .self
	@for c in *.c; do \
		echo CC -D__mocc_self__ -P -E "$$c" -o ".self/$${c%.c}.c"; \
		$(CC) -D__mocc_self__ -P -E "$$c" -o ".self/$${c%.c}.c"; \
	done

mocc-stage2.riscv: mocc-stage1 preprocess
	@echo "# stage2: mocc on RISC-V machine generated by stage1 (mocc on host machine)"
	@rm -rf .stage2
	@mkdir .stage2
	@for c in .self/*.c; do \
		echo MOCC-STAGE1 "$$c > $${c/.self/.stage2}.s"; \
		./mocc-stage1 "$$c" > "$${c/.self/.stage2}.s"; \
	done
	riscv64-$(RISCV_HOST)-gcc -static .stage2/*.s -o ./mocc-stage2.riscv
	riscv64-$(RISCV_HOST)-strip ./mocc-stage2.riscv

test-stage2: mocc-stage2.riscv
	./riscvw ./mocc-stage2.riscv test/test.c > tmp.s
	riscv64-$(RISCV_HOST)-gcc -static tmp.s test/helper.c -o test.riscv
	prove -v -e ./riscvw ./test.riscv

mocc-stage3.riscv: mocc-stage2.riscv
	@echo "# stage3: mocc on RISC-V machine generated by stage2 (mocc on RISC-V machine)"
	@rm -rf .stage3
	@mkdir .stage3
	@for c in .self/*.c; do \
		echo MOCC-STAGE2 "$$c > $${c/.self/.stage3}.s"; \
		./riscvw ./mocc-stage2.riscv "$$c" > "$${c/.self/.stage3}.s"; \
	done
	riscv64-$(RISCV_HOST)-gcc -static .stage3/*.s -o ./mocc-stage3.riscv

test-stage3: mocc-stage3.riscv
	./riscvw ./mocc-stage3.riscv test/test.c > tmp.s
	riscv64-$(RISCV_HOST)-gcc -static tmp.s test/helper.c -o test.riscv
	prove -v -e ./riscvw ./test.riscv

clean:
	rm -rf mocc *.o *~ tmp* *.gcov *.gcda *.gcno *.profraw *.profdata coverage.html .self .stage*

.PHONY: test clean

# cc -MM -MF - *.c
mocc.o: mocc.c mocc.h
codegen.o: codegen.c mocc.h
parse.o: parse.c mocc.h
tokenize.o: tokenize.c mocc.h
type.o: type.c mocc.h
util.o: util.c mocc.h
