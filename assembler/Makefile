FSANITIZE ?= 1
RELEASE ?= 0

CC := gcc

CFLAGS := -Wall -Wextra -g -fsanitize=undefined -fsanitize=address
ifeq ($(FSANITIZE), 0)
    CFLAGS := -Wall -Wextra -g
endif
ifeq ($(RELEASE),1)
	CFLAGS := -Wall -Wextra -Ofast
endif

asm.elf: main.o types.o vec.o str.o parser.o util.o register.o gen.o syntax.o
	$(CC) $(CFLAGS) $^ -o $@

main.o: main.c types.h parser.h syntax.h
types.o: types.h
parser.o: parser.c parser.h util.h
vec.o: vec.c vec.h types.h
str.o: str.c str.h vec.h
util.o: util.c util.h types.h
register.o: register.c register.h types.h
gen.o: gen.c types.h parser.h gen.h
syntax.o: syntax.c types.h gen.h syntax.h

types.h:
parser.h: types.h
str.h: types.h vec.h
vec.h: types.h
util.h: types.h
register.h: types.h parser.h
gen.h: types.h register.h vec.h
syntax.h: types.h gen.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: asm.elf
	./asm.elf

debug: asm.elf
	gdb asm.elf

clean:
	rm -f *.elf *.o
