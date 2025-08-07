#!/usr/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"
asmacro std.amc
asmacro core.amc
asmacro std/parser.amc

ar rcs std.a std.o std/parser.o
ar rcs core.a core.o

