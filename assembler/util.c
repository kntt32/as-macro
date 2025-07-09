#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "types.h"
#include "util.h"

u64 get_file_size(FILE* stream) {
    assert(stream != NULL);

    if(fseek(stream, 0, SEEK_END)) {
        PANIC("fseek returned error");
    }
    u64 size = ftell(stream);
    if(fseek(stream, 0, SEEK_SET)) {
        PANIC("fseek returned error");
    }

    return size;
}

i32 floor_div(i32 x, i32 y) {
    i32 q = x/y;
    if((x < 0 && 0 < y) || (0 < x && y < 0)) {
        if(x % y != 0) {
            q --;
        }
    }

    return q;
}

i32 ceil_div(i32 a, i32 b) {
    i32 q = a / b;
    if ((a % b != 0) && ((a > 0 && b > 0) || (a < 0 && b < 0))) {
        q++;
    }

    return q;
}

