LINKER ?= ld
ASMACRO = ../assembler/asm.elf

a.out: main.o register.o type.o storage.o $(ASMACRO_STDLIBS)/std.a $(ASMACRO_STDLIBS)/core.o
	$(LINKER) $^ -o $@

main.o: main.amc register.amc gen.amc global_syntax.amc type.amc
storage.o: storage.amc register.amc
register.o: register.amc
type.o: type.amc

main.amc: 

%.o: %.amc
	$(ASMACRO) $< -o $@

run: a.out
	./a.out

debug: a.out
	gdb a.out

clean:
	rm -f *.o *.out *.elf

init:
	export ASMACRO_STDLIBS=$(HOME)/as-macro/test

