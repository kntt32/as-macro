LINKER ?= ld
ASMACRO = ../assembler/asm.elf

a.out: main.o std.a core.o
	$(LINKER) $^ -o $@

std.a: std.o parser.o
	ar rcs $@ $^

main.o: main.amc
std.o: std.amc
parser.o: std/parser.amc
core.o: core.amc

main.amc: std.amc
std.amc: core.amc
core.amc: 

%.o: %.amc
	asmacro $< -o $@

%.o: std/%.amc
	asmacro $< -o $@

%.h:
	touch $@ -c

run: a.out
	./a.out

debug: a.out
	gdb a.out

clean:
	rm -f *.o *.out *.elf *.a

