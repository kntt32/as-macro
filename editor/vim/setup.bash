#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"
cp -r ./ftdetect ~/.vim/
cp -r ./ftplugin ~/.vim/
cp -r ./syntax ~/.vim/

