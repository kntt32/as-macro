#!/bin/bash

../assembler/asm.elf main.amc
../assembler/asm.elf parser.amc
ld main.o parser.o ../stdlibs/std.o ../stdlibs/core.o -o a.out

