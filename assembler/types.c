#include <stdio.h>
#include <stdbool.h>
#include "types.h"

SResult SRESULT_OK = {true, ""};

void u8_print_for_vec(in void* ptr) {
    u8* u8_ptr = ptr;
    printf("%u", *u8_ptr);
}

