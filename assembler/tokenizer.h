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
    TokenType_String,
    TokenType_Character,
    TokenType_Whitespace,
    TokenType_Eof,
    TokenType_Error,
} TokenType;

typedef enum {
    TokenError_InvalidStringLiteral,
    TokenError_InvalidCharLiteral,
    TokenError_UnknownEscape,
} TokenError;

typedef struct {
    TokenType type;
    union {
        char keyword[256];
        char symbol;
        Vec string;// Vec<char>
        char character;
        TokenError error;
    } body;
    Offset offset;
} Token;

typedef struct {
    Vec tokens;// Vec<Token>
} TokenField;

Offset Offset_new(in char* path);
bool Offset_cmp(in Offset* self, in Offset* other);
void Offset_seek_char(inout Offset* self, char c);
void Offset_seek(inout Offset* self, char* token);
void Offset_print(in Offset* self);

void TokenType_print(TokenType self);

char* TokenError_as_str(TokenError self);

Token Token_from(inout char** src, inout Offset* offset);
void Token_print(in Token* self);
void Token_print_for_vec(in void* ptr);
void Token_free(Token self);
void Token_free_for_vec(inout void* ptr);

TokenField TokenField_new(in char* src, in char* path);
void TokenField_print(in TokenField* self);
void TokenField_free(TokenField self);
