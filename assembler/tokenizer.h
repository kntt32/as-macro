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
} TokenType;

typedef struct {
    TokenType type;
    union {
        char keyword[256];
        char symbol;
        char* string;// owned string
        char character;
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

Token Token_from(char** src, Offset* offset);
void Token_print(Token* self);
void Token_print_for_vec(void* ptr);
void Token_free(Token self);
void Token_free_for_vec(void* ptr);

TokenField TokenField_new(char* src, char* path);
void TokenField_free(TokenField self);
