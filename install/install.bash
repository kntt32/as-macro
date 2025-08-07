#!/usr/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"
mkdir "$HOME/.asmacro/" -p
mkdir "$HOME/.asmacro/bin/" -p
cp asmacro "$HOME/.asmacro/bin/asmacro"
cp ../stdlibs/std "$HOME/.asmacro/std" -r -p
cp ../stdlibs/std.amc "$HOME/.asmacro/std.amc" -p
cp ../stdlibs/core "$HOME/.asmacro/core" -r -p
cp ../stdlibs/core.amc "$HOME/.asmacro/core.amc" -p
cp ../stdlibs/buildstdlibs.bash "$HOME/.asmacro/buildstdlibs.bash" -p
echo 'export ASMACRO_STDLIBS="$HOME/.asmacro/"' >> "$HOME/.bashrc"
echo 'export PATH="$HOME/.asmacro/bin:$PATH"' >> "$HOME/.bashrc"
source ~/.bashrc
source "$HOME/.asmacro/buildstdlibs.bash"

