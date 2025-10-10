#include <stdio.h>
#include <stdbool.h>
#include "types.h"
#include "util.h"

static void char_free(char self) {
    // nothing to do
}

DeriveOptionMethods(char)

SResult SResult_new(optional char* error) {
    SResult result = {""};
    if(error != NULL) {
        wrapped_strcpy(result.error, error, sizeof(result.error));
    }

    return result;
}

bool SResult_is_success(SResult self) {
    return self.error[0] == '\0';
}

bool SResult_is_success_ptr(in SResult* self) {
    return self->error[0] == '\0';
}

void char_print_for_vec(in void* ptr) {
    char* char_ptr = ptr;
    printf("%c", *char_ptr);
}

void u8_print_for_vec(in void* ptr) {
    u8* u8_ptr = ptr;
    printf("%x", *u8_ptr);
}

bool u8_cmp_for_vec(in void* ptr1, in void* ptr2) {
    u8* val1 = ptr1;
    u8* val2 = ptr2;
    return *val1 == *val2;
}

