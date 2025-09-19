#pragma once

#include "types.h"
#include "vec.h"

typedef struct {
    char path[256];
    u32 line;
    u32 column;
} Offset;

typedef enum {
    TokenType_Keyword,
    TokenType_Symbol,
    TokenType_Number,
    TokenType_String,
    TokenType_Character,
} TokenType;

typedef struct {
    TokenType type;
    union {
        char keyword[256];
        char symbol;
        i64 number;
        char* string;// owned string
        char character;
    } body;
    Offset offset;
} Token;

typedef struct {
    char path[4096];
    Vec tokens;// Vec<Token>
} TokenVec;

