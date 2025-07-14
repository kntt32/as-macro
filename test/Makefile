LINKER ?= ld
ASMACRO = ../assembler/asm.elf

a.out: main.o std.o core.o
	$(LINKER) $^ -o $@

main.o: main.amc
std.o: std.amc
core.o: core.amc

main.amc: std.amc
std.amc: core.amc
core.amc: core_x64.amc

%.o: %.amc
	$(ASMACRO) $< -o $@

%.h:
	touch $@ -c

run: a.out
	./a.out

debug: a.out
	gdb a.out

clean:
	rm -f *.o *.out *.elf

