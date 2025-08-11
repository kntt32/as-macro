#!/usr/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"
rm "$HOME/.asmacro/" -rf
mkdir "$HOME/.asmacro/" -p
mkdir "$HOME/.asmacro/bin/" -p
cp asmacro "$HOME/.asmacro/bin/asmacro"
cp ../stdlibs/std "$HOME/.asmacro/std" -r
cp ../stdlibs/std.amc "$HOME/.asmacro/std.amc"
cp ../stdlibs/core "$HOME/.asmacro/core" -r
cp ../stdlibs/core.amc "$HOME/.asmacro/core.amc"
cp ../stdlibs/buildstdlibs.bash "$HOME/.asmacro/buildstdlibs.bash"
echo >> "$HOME/.bashrc"
echo 'export ASMACRO_STDLIBS="$HOME/.asmacro/"' >> "$HOME/.bashrc"
echo 'export PATH="$HOME/.asmacro/bin:$PATH"' >> "$HOME/.bashrc"
echo >> "$HOME/.bashrc"
export ASMACRO_STDLIBS="$HOME/.asmacro/"
export PATH="$HOME/.asmacro/bin:$PATH"
source "$HOME/.asmacro/buildstdlibs.bash"

