#! /usr/bin/bash

asmacro main.amc -o main.o
ld main.o "$ASMACRO_STDLIBS/core.a" "$ASMACRO_STDLIBS/std.a"
./a.out

