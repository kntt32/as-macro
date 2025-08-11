#!/usr/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"
./bin/asmacro std.amc
./bin/asmacro core.amc
./bin/asmacro std/parser.amc

ar rcs std.a std.o std/parser.o
ar rcs core.a core.o

