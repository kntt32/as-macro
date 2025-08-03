#!/bin/bash

../assembler/asm.elf main.amc
ld main.o ../stdlibs/std.o ../stdlibs/core.o -o a.out

