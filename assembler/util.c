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

