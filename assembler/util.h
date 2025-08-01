#pragma once

#include <stdio.h>
#include "types.h"

u64 get_file_size(FILE* stream);
SResult map_file(in char* path, out char** mem);

i32 floor_div(i32 x, i32 y);
i32 ceil_div(i32 a, i32 b);

u32 get_id();

void wrapped_strcat(inout char* dst, in char* src, u64 buff_len);
void wrapped_strcpy(out char* dst, in char* src, u64 buff_len);

