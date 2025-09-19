#include "tokenizer.h"
#include <assert.h>
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
        case TokenType_Number:
            printf("TokenType_Number");
            break;
        case TokenType_String:
            printf("TokenType_String");
            break;
        case TokenType_Character:
            printf("TokenType_Character");
            break;
    }
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
        case TokenType_Number:
            printf(".number: %ld", self->body.number);
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

void Token_free(Token self) {
    TODO();
}
