#include <stdio.h>
#include <stdbool.h>
#include "types.h"

SResult SResult_new(optional char* error) {
    SResult result = {""};
    if(error != NULL) {
        strncpy(result.error, error, 256);
        result.error[255] = '\0';
    }

    return result;
}

bool SResult_is_success(SResult self) {
    return self.error[0] == '\0';
}

void u8_print_for_vec(in void* ptr) {
    u8* u8_ptr = ptr;
    printf("%x", *u8_ptr);
}

