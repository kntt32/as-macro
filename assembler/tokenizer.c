#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include "tokenizer.h"
#include "util.h"

Offset Offset_new(in char* path) {
    assert(path != NULL);

    Offset offset;

    wrapped_strcpy(offset.path, path, sizeof(offset.path));
    offset.line = 1;
    offset.column = 1;

    return offset;
}

bool Offset_cmp(in Offset* self, in Offset* other) {
    assert(self != NULL && other != NULL);
    return strcmp(self->path, other->path) == 0
        && self->line == other->line
        && self->column == other->column;
}

void Offset_seek_char(inout Offset* self, char c) {
    assert(self != NULL);

    switch(c) {
        case '\n':
            self->line ++;
            self->column = 1;
            break;
        case '\0':
            break;
        default:
            self->column ++;
            break;
    }
}

void Offset_seek(inout Offset* self, char* token) {
    assert(self != NULL);
    assert(token != NULL);

    u32 token_len = strlen(token);

    for(u32 i=0; i<token_len; i++) {
        Offset_seek_char(self, token[i]);
    }
}

void Offset_print(in Offset* self) {
    assert(self != NULL);
    printf("Offset { path: %s, line: %u, column: %u }", self->path, self->line, self->column);
}

void TokenType_print(TokenType self) {
    switch(self) {
        case TokenType_Keyword:
            printf("TokenType_Keyword");
            break;
        case TokenType_Symbol:
            printf("TokenType_Symbol");
            break;
        case TokenType_String:
            printf("TokenType_String");
            break;
        case TokenType_Character:
            printf("TokenType_Character");
            break;
    }
}

static Token Token_from_keyword(char** src, Offset* offset) {
    TODO();
    Token token;
    return token;
}

Token Token_from(char** src, Offset* offset) {
    char c = **src;
    unsigned char unsigned_c = c;
    Token token;

    if(isalpha(unsigned_c) || isdigit(unsigned_c)) {
        token = Token_from_keyword(src, offset);
    }else {
        TODO();
    }

    return token;
}

void Token_print(Token* self) {
    printf("Token { type: ");
    TokenType_print(self->type);
    printf(", body: ");
    switch(self->type) {
        case TokenType_Keyword:
            printf(".keyword: %s", self->body.keyword);
            break;
        case TokenType_Symbol:
            printf(".symbol: %c", self->body.symbol);
            break;
        case TokenType_String:
            printf(".string: %s", self->body.string);
            break;
        case TokenType_Character:
            printf(".character: %c", self->body.character);
            break;
    }
    printf(", offset: ");
    Offset_print(&self->offset);
    printf(" }");
}

void Token_print_for_vec(void* ptr) {
    Token_print(ptr);
}

void Token_free(Token self) {
    switch(self.type) {
        case TokenType_Keyword:
        case TokenType_Symbol:
        case TokenType_Character:
            break;
        case TokenType_String:
            free(self.body.string);
            break;
    }
}

void Token_free_for_vec(void* ptr) {
    Token* this = ptr;
    Token_free(*this);
}

TokenField TokenField_new(char* src, char* path) {
    Vec tokens = Vec_new(sizeof(Token));// Vec<Token>
    Offset offset = Offset_new(path);

    while(*src != '\0') {
        char c = *src;
        Token token = Token_from(&src, &offset);
        Vec_push(&tokens, &token);
    }

    TokenField this;
    this.tokens = tokens;

    return this;
}

void TokenField_print(TokenField* self) {
    printf("TokenField { tokens: ");
    Vec_print(&self->tokens, Token_print_for_vec);
    printf(" }");
}

void TokenField_free(TokenField self) {
    Vec_free_all(self.tokens, Token_free_for_vec);
}
