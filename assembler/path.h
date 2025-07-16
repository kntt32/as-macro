#pragma once

#include "types.h"

char* path_real(in char* path, out char buffer[4096]);
char* path_super(inout char path[4096]);
char* path_child(inout char path[4096], in char* child);
char* path_append_extension(inout char path[4096], in char* extension);

