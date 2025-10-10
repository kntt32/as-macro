#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include "tokenizer.h"
#include "util.h"

static struct { char expr; char code; } ESCAPE_SEQUENCES[] = {
    {'\\', '\\'}, {'\'', '\''}, {'\"', '\"'}, {'?', '?'}, {'a', '\a'}, {'b', '\b'},
    {'f', '\f'}, {'n', '\n'}, {'r', '\r'}, {'t', '\t'}, {'v', '\v'}, {'0', '\0'},
};

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
        case TokenType_Whitespace:
            printf("TokenType_Whitespace");
            break;
        case TokenType_Eof:
            printf("TokenType_Eof");
            break;
        case TokenType_Error:
            printf("TokenType_Error");
            break;
    }
}

char* TokenError_as_str(TokenError self) {
    static struct {TokenError error; char* string;} TABLE[] = {
        {TokenError_UnknownEscape, "UnknownEscape"},
        {TokenError_InvalidCharLiteral, "InvalidCharLiteral"},
        {TokenError_InvalidStringLiteral, "InvalidStringLiteral"},
        {TokenError_InvalidCodepoint, "InvalidCodepoint"},
    };

    for(u32 i=0; i<LEN(TABLE); i++) {
        if(TABLE[i].error == self) {
            return TABLE[i].string;
        }
    }

    PANIC("unreachable");
}

static char Token_look(in char** src) {
    return **src;
}

static char Token_read(inout char** src, inout Offset* offset) {
    char c = Token_look(src);
    if(c != '\0') {
        Offset_seek_char(offset, c);
        (*src) ++;
    }
    return c;
}

static Token Token_from_keyword(inout char** src, inout Offset* offset) {
    u32 len = 0;
    Token token = {TokenType_Keyword, {.keyword = ""}, *offset};

    while(len < sizeof(token.body.keyword)) {
        char c = Token_look(src);
        unsigned char unsigned_c = c;

        if(c == '\0' || !isascii(unsigned_c) || !(isalpha(unsigned_c) || isdigit(unsigned_c))) {
            token.body.keyword[len] = '\0';
            break;
        }

        token.body.keyword[len] = Token_read(src, offset);
        len ++;
    }

    token.body.keyword[255] = '\0';
    return token;
}

static Token Token_from_symbol(inout char** src, inout Offset* offset) {
    static char* SYMBOLS[] = {"(", ")", "[", "]", "{", "}", ";", "="};

    char* start = *src;
    while(ispunct((unsigned char)Token_look(src))) {
        bool flag = false;
        for(u32 i=0; i<LEN(SYMBOLS); i++) {
            if(strncmp(SYMBOLS[i], start, *src + 1 - start) == 0) {
                flag = true;
                break;
            }
        }
        if(!flag) {
            if(*src == start) {
                Token error_token = {TokenType_Error, {.error = TokenError_InvalidCodepoint}, *offset};
                Token_read(src, offset);
                return error_token;
            }else {
                break;
            }
        }

        Token_read(src, offset);
    }

    if(start == *src) {
        Token error_token = {TokenType_Error, {.error = TokenError_InvalidCodepoint}, *offset};
        Token_read(src, offset);
        return error_token;
    }

    Token token = {TokenType_Symbol, {.symbol = ""}, *offset};
    u64 len = *src - start;
    if(256 <= len) {
        len = 255;
    }
    memcpy(token.body.symbol, start, len);
    token.body.symbol[len] = '\0';

    return token;
}

static Option_char Token_unescape(char c) {
    for(u32 i=0; i<LEN(ESCAPE_SEQUENCES); i++) {
        if(ESCAPE_SEQUENCES[i].expr == c) {
            return Option_char_some(ESCAPE_SEQUENCES[i].code);
        }
    }

    return Option_char_none();
}

static Token Token_from_character(inout char** src, inout Offset* offset) {
    assert(Token_look(src) == '\'');
    Token_read(src, offset);

    Token token = {TokenType_Character, {.character = '\0'}, *offset};

    char c = Token_read(src, offset);
    if(c == '\\') {
        Option_char optional_c = Token_unescape(c);
        if(Option_char_is_some(&optional_c)) {
            c = Option_char_unwrap(optional_c);
        }else {
            Token error_token = {TokenType_Error, {.error = TokenError_UnknownEscape}, *offset};
            return error_token;
        }
    }
    if(Token_read(src, offset) != '\'') {
        Token error_token = {TokenType_Error, {.error = TokenError_InvalidCharLiteral}, *offset};
        return error_token;
    }
    token.body.character = c;

    return token;
}

static Token Token_from_string(inout char** src, inout Offset* offset) {
    assert(Token_look(src) == '\"');
    Token_read(src, offset);

    Token token = {TokenType_String, {.string = Vec_new(sizeof(char))}, *offset};

    loop {
        char c = Token_read(src, offset);

        switch(c) {
            case '\"':
                return token;
            case '\0':
                {
                    Token error_token = {TokenType_Error, {.error = TokenError_InvalidStringLiteral}, *offset};
                    Token_free(token);
                    return error_token;
                }
            case '\\':
                {
                    Option_char optional_c = Token_unescape(Token_read(src, offset));
                    if(Option_char_is_some(&optional_c)) {
                        char c = Option_char_unwrap(optional_c);
                        Vec_push(&token.body.string, &c);
                    }else {
                        Token error_token = {TokenType_Error, {.error = TokenError_UnknownEscape}, *offset};
                        Token_free(token);
                        return error_token;
                    }
                }
                break;
            default:
                Vec_push(&token.body.string, &c);
                break;
        }
    }

    PANIC("unreachable");
    return token;
}

static void Token_skip_space(inout char** src, inout Offset* offset) {
    while(**src != '\0' && isspace((unsigned char)**src)) {
        Token_read(src, offset);
    }
}

Token Token_from(inout char** src, inout Offset* offset) {
    char c = Token_look(src);
    unsigned char unsigned_c = c;
    Token token;

    if(isspace(c)) {
        Token whitespace_token = {TokenType_Whitespace, {}, *offset};
        Token_skip_space(src, offset);
        return whitespace_token;
    }
    if(c == '\0') {
        Token eof_token = {TokenType_Eof, {}, *offset};
        return eof_token;
    }

    if(isalpha(unsigned_c) || isdigit(unsigned_c)) {
        token = Token_from_keyword(src, offset);
    }else if(c != '\'' && c != '\"') {
        token = Token_from_symbol(src, offset);
    }else if(c == '\"') {
        token = Token_from_string(src, offset);
    }else if(c == '\'') {
        token = Token_from_character(src, offset);
    }else {
        PANIC("unreachable");
    }

    return token;
}

void Token_print(in Token* self) {
    printf("Token { type: ");
    TokenType_print(self->type);
    printf(", body: ");
    switch(self->type) {
        case TokenType_Keyword:
            printf(".keyword: %s", self->body.keyword);
            break;
        case TokenType_Symbol:
            printf(".symbol: %s", self->body.symbol);
            break;
        case TokenType_String:
            printf(".string: ");
            for(u32 i=0; i<Vec_len(&self->body.string); i++) {
                printf("%c", *(char*)Vec_index(&self->body.string, i));
            }
            break;
        case TokenType_Character:
            printf(".character: %c", self->body.character);
            break;
        case TokenType_Whitespace:
        case TokenType_Eof:
            printf("none");
            break;
        case TokenType_Error:
            printf(".error: %s", TokenError_as_str(self->body.error));
            break;
    }
    printf(", offset: ");
    Offset_print(&self->offset);
    printf(" }");
}

void Token_print_for_vec(in void* ptr) {
    Token_print(ptr);
}

void Token_free(Token self) {
    switch(self.type) {
        case TokenType_Keyword:
        case TokenType_Symbol:
        case TokenType_Character:
        case TokenType_Whitespace:
        case TokenType_Eof:
        case TokenType_Error:
            break;
        case TokenType_String:
            Vec_free(self.body.string);
            break;
    }
}

void Token_free_for_vec(inout void* ptr) {
    Token* this = ptr;
    Token_free(*this);
}

TokenField TokenField_new(in char* src, in char* path) {
    Vec tokens = Vec_new(sizeof(Token));// Vec<Token>
    Offset offset = Offset_new(path);

    loop {
        Token token = Token_from(&src, &offset);
        TokenType type = token.type;
        Vec_push(&tokens, &token);
        if(type == TokenType_Eof) {
            break;
        }
    }

    TokenField this = {tokens, offset};

    return this;
}

void TokenField_print(in TokenField* self) {
    printf("TokenField { tokens: ");
    Vec_print(&self->tokens, Token_print_for_vec);
    printf(" }");
}

void TokenField_free(TokenField self) {
    Vec_free_all(self.tokens, Token_free_for_vec);
}
