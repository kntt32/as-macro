#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef float f32;
typedef double f64;

typedef struct {
    bool ok_flag;
    char error[256];
} SResult;

extern SResult SRESULT_OK;

#define in
#define out
#define inout
#define optional
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
#define TODO() (PANIC("not implemented"))
#define LEN(arr) (sizeof(arr)/sizeof(arr[0]))
#define OWN_STR(str) (\
    char* ptr = malloc((strlen(str) + 1) * sizeof(char));\
    strcpy(ptr, str);\
    ptr\
)
#define MAX(x, y) (x > y)?(x):(y)
#define BOOL_TO_STR(b) (b)?("true"):("false")
#define SRESULT_IS_OK(r) (r.ok_flag)
#define SRESULT_UNWRAP(r, catch_proc) {\
    SResult result = r;\
    if(!SRESULT_IS_OK(result)) {\
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

