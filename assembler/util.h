#pragma once

#include "types.h"

char* Util_trim_str(in char* str);

u64* Util_str_to_u64(in char* str, out u64* ptr);

bool Util_is_number(in char* str);

