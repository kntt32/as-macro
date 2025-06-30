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
    if(!SResult_is_success(result)) {\
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

void u8_print_for_vec(in void* ptr);


