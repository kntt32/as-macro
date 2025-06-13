#pragma once

#include "types.h"
#include "vec.h"

typedef struct {
    Vec vec;
} String;

String String_new();

void String_free(String self);

void String_append(String* self, char* src);

char* String_as_ptr(String* self);

void String_print(in String* self);

