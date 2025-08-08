#!/bin/bash

asmacro main.amc
asmacro parser.amc
ld main.o parser.o ../stdlibs/std.o ../stdlibs/core.o -o a.out

