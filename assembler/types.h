#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef struct {
    char error[256];
} SResult;

#define DeriveOption(type)\
    typedef struct {\
        bool is_some;\
        union { type some; } body;\
    } Option_##type;\
\
    Option_##type Option_##type##_some(type value);\
    Option_##type Option_##type##_none(void);\
    bool Option_##type##_is_some(Option_##type* self);\
    bool Option_##type##_is_none(Option_##type* self);\
    type Option_##type##_unwrap(Option_##type self);\
    void Option_##type##_free(Option_##type self);\

#define DeriveOptionMethods(type)\
    Option_##type Option_##type##_some(type value) {\
        Option_##type this = {true, {.some = value}};\
        return this;\
    }\
\
    Option_##type Option_##type##_none(void) {\
        Option_##type this = {false, {}};\
        return this;\
    }\
\
    bool Option_##type##_is_some(Option_##type* self) {\
        return self->is_some;\
    }\
\
    bool Option_##type##_is_none(Option_##type* self) {\
        return !self->is_some;\
    }\
\
    type Option_##type##_unwrap(Option_##type self) {\
        if(!self.is_some) {\
            PANIC("expected some");\
        }\
        return self.body.some;\
    }\
\
    void Option_##type##_free(Option_##type self) {\
        if(self.is_some) {\
            type##_free(self.body.some);\
        }\
    }\

DeriveOption(char)

#define in
#define out
#define inout
#define optional
#define pub
#define PANIC(msg)\
{\
    fflush(NULL);\
    if(strlen(msg) == 0) {\
        fprintf(stderr, "%s:%d: paniced\n", __FILE__, __LINE__);\
    }else {\
        fprintf(stderr, "%s:%d:paniced:%s\n", __FILE__, __LINE__, msg);\
    }\
    exit(-1);\
}
#define TODO() PANIC("not implemented")
#define LEN(arr) (sizeof(arr)/sizeof(arr[0]))
#define OWN_STR(str) (\
    char* ptr = malloc((strlen(str) + 1) * sizeof(char));\
    strcpy(ptr, str);\
    ptr\
)
#define MAX(x, y) (x > y)?(x):(y)
#define BOOL_TO_STR(b) (b)?("true"):("false")
#define SRESULT_UNWRAP(r, catch_proc) {\
    SResult result = r;\
    if(!SResult_is_success_ptr(&result)) {\
        catch_proc;\
        return result;\
    }\
}
#define UNWRAP_NULL(ptr) {\
    if(ptr == NULL) {\
        PANIC("nullptr was unwrapped");\
    }\
}
#define loop while(true)

SResult SResult_new(optional char* error);
bool SResult_is_success(SResult self);
bool SResult_is_success_ptr(in SResult* self);

void u8_print_for_vec(in void* ptr);
bool u8_cmp_for_vec(in void* ptr1, in void* ptr2);

void char_print_for_vec(in void* ptr);

