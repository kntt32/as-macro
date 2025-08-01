FSANITIZE ?= 1
RELEASE ?= 0

CC ?= gcc

CFLAGS := -Wall -Wextra -O0 -g -fsanitize=undefined -fsanitize=address -fsanitize=leak
ifeq ($(FSANITIZE), 0)
    CFLAGS := -Wall -Wextra -O0 -g
endif
ifeq ($(RELEASE),1)
	CFLAGS := -Wall -Wextra -Ofast -flto -g
endif

asm.elf: main.o types.o vec.o parser.o util.o register.o gen.o gen_type.o gen_encode.o gen_storage.o gen_data.o gen_variable.o gen_asmacro.o gen_elf.o gen_template.o syntax.o global_syntax.o cmd.o path.o
	$(CC) $(CFLAGS) $^ -o $@

main.o: main.c types.h cmd.h
types.o: types.c types.h util.h
parser.o: parser.c parser.h util.h
vec.o: vec.c vec.h types.h
util.o: util.c util.h types.h
register.o: register.c register.h types.h
gen.o: gen.c types.h register.h parser.h gen.h path.h util.h
gen_type.o: gen_type.c gen.h util.h
gen_encode.o: gen_type.c gen.h
gen_storage.o: gen_storage.c gen.h util.h
gen_data.o: gen_data.c gen.h util.h
gen_asmacro.o: gen_asmacro.c gen.h
gen_variable.o: gen_variable.c gen.h util.h
gen_template.o: gen_template.c gen.h types.h vec.h parser.h util.h
gen_elf.o: gen_elf.c gen_elf.h util.h
syntax.o: syntax.c types.h gen.h syntax.h util.h
global_syntax.o: global_syntax.c global_syntax.h util.h
cmd.o: cmd.c types.h vec.h gen.h gen_elf.h global_syntax.h cmd.h util.h
path.o: path.c path.h types.h

types.h:
parser.h: types.h vec.h
vec.h: types.h
util.h: types.h
register.h: types.h parser.h
gen.h: types.h register.h vec.h
gen_elf.h: types.h vec.h gen.h
syntax.h: types.h gen.h
global_syntax.h: syntax.h
cmd.h: types.h vec.h gen.h
path.h: types.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.h:
	touch $@ -c

run: asm.elf
	./asm.elf $(ARGS)

debug: asm.elf
	gdb asm.elf

clean:
	rm -f *.elf *.o *.out

