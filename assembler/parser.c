#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "parser.h"
#include "util.h"

Offset Offset_new(in char* path) {
    Offset offset;

    strncpy(offset.path, path, 256);
    offset.path[255] = '\0';
    offset.line = 1;
    offset.column = 1;

    return offset;
}

void Offset_seek_char(inout Offset* self, char c) {
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
    u32 token_len = strlen(token);

    for(u32 i=0; i<token_len; i++) {
        Offset_seek_char(self, token[i]);
    }
}

void Offset_print(in Offset* self) {
    printf("Offset { path: %s, line: %u, column: %u }", self->path, self->line, self->column);
}

Parser Parser_new(in char* src, in char* path) {
    Parser parser = {src, strlen(src), Offset_new(path)};
    return parser;
}

static char Parser_read(inout Parser* self) {
    if(self->len == 0) {
        return '\0';
    }

    char c = self->src[0];
    Offset_seek_char(&self->offset, c);
    self->src ++;
    self->len --;

    return c;
}

static void Parser_skip_space(inout Parser* self) {
    while(isspace(self->src[0]) && self->len != 0) {
        Parser_read(self);
    }
}

static bool Parser_is_gap(in Parser* self) {
    char c = self->src[0];
    return self->src[0] != '\0' && (isspace(c) || (ispunct(c) && c != '_'));
}

static void Parser_run_for_gap(inout Parser* self, out char token[256]) {
    Parser_skip_space(self);
    u32 len = 0;
    
    while(isascii(self->src[0]) && !Parser_is_gap(self) && len < 256 - 1) {
        token[len] = Parser_read(self);
        len ++;
    }

    token[len] = '\0';

    return;
}

static bool Parser_skip(inout Parser* self) {
    Parser_skip_space(self);

    if(Parser_is_empty(self)) {
        return false;
    }

    char c[3];
    if(ParserMsg_is_success(Parser_parse_charliteral(self, c))) {
        return true;
    }
    
    char token[256];
    Parser_run_for_gap(self, token);
    if(token[0] != '\0') {
        return true;
    }

    static ParserMsg (*BLOCK_PARSERS[3])(Parser*, Parser*) = {Parser_parse_block, Parser_parse_paren, Parser_parse_index};
    for(u32 i=0; i<3; i++) {
        Parser parser;
        if(ParserMsg_is_success(BLOCK_PARSERS[i](self, &parser))) {
            return true;
        }
    }

    if(ispunct(self->src[0])) {
        Parser_read(self);
        return true;
    }

    return false;
}

static ParserMsg Parser_parse_block_helper(inout Parser* self, out Parser* parser, in char* start, in char* end) {
    Parser self_copy = *self; 

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&self_copy, start),
        (void)(NULL)
    );

    *parser = self_copy;

    while(!ParserMsg_is_success(Parser_parse_symbol(&self_copy, end))) {
        if(!Parser_skip(&self_copy)) {
            PANIC("unknown token");
        }
    }

    parser->len = parser->len - self_copy.len - 1;
    *self = self_copy;

    return ParserMsg_new(self->offset, NULL);
}

bool Parser_is_empty(in Parser* self) {
    Parser_skip_space(self);
    return 0 == self->len;
}

void Parser_skip_to_semicolon(inout Parser* self) {
    while(!ParserMsg_is_success(Parser_parse_symbol(self, ";"))) {
        Parser_skip(self);
    }

    return;
}

bool Parser_start_with(inout Parser* self, in char* keyword) {
    Parser self_copy = *self;

    return ParserMsg_is_success(Parser_parse_keyword(&self_copy, keyword));
}

bool Parser_start_with_symbol(inout Parser* self, in char* symbol) {
    Parser self_copy = *self;

    return ParserMsg_is_success(Parser_parse_symbol(&self_copy, symbol));
}

Parser Parser_split(inout Parser* self, in char* symbol) {
    Parser self_copy = *self;
    Parser parser = *self;

    if(Parser_is_empty(self)) {
        return parser;
    }

    while(!ParserMsg_is_success(Parser_parse_symbol(&self_copy, symbol))) {
        if(!Parser_skip(&self_copy)) {
            if(Parser_is_empty(&self_copy)) {
                break;
            }else {
                PANIC("unknown token");
            }
        }
        
        parser.len = self->len - self_copy.len;
    }

    *self = self_copy;

    return parser;
}

char* Parser_own(in Parser* parser, out Parser* owned_parser) {
    char* src = malloc(sizeof(char) * (parser->len + 1));
    UNWRAP_NULL(src);
    memcpy(src, parser->src, parser->len);
    src[parser->len] = '\0';
    
    owned_parser->src = src;
    owned_parser->len = parser->len;
    owned_parser->offset = parser->offset;

    return src;
}

ParserMsg Parser_parse_ident(inout Parser* self, out char token[256]) {
    Parser self_copy = *self;
    Parser_run_for_gap(&self_copy, token);
    u32 len = strlen(token);

    ParserMsg msg = ParserMsg_new(self->offset, "expected identifier");
    
    if(len == 0) {
        return msg;
    }

    for(u32 i=0; i<len; i++) {
        if(!(isascii(token[i]) && (token[i] == '_' || isalpha(token[i]) || (i != 0 && isalnum(token[i]))))) {
            return msg;
        }
    }

    *self = self_copy;

    return ParserMsg_new(self->offset, NULL);
}

ParserMsg Parser_parse_keyword(inout Parser* self, in char* keyword) {
    Parser self_copy = *self;
    char token[256];
    Parser_run_for_gap(&self_copy, token);
    if(strcmp(token, keyword) != 0) {
        return ParserMsg_new(self->offset, "expected keyword");
    }

    *self = self_copy;

    return ParserMsg_new(self->offset, NULL);
}

ParserMsg Parser_parse_symbol(inout Parser* self, in char* symbol) {
    Parser_skip_space(self);

    u32 symbol_len = strlen(symbol);
    if(strncmp(self->src, symbol, symbol_len) != 0) {
        return ParserMsg_new(self->offset, "expected symbol");
    }

    for(u32 i=0; i<symbol_len; i++) {
        Parser_read(self);
    }

    return ParserMsg_new(self->offset, NULL);
}

ParserMsg Parser_parse_number(inout Parser* self, out u64* value) {
    Parser self_copy = *self;

    bool minus_flag = ParserMsg_is_success(Parser_parse_symbol(self, "-"));

    char token[256];
    
    Parser_run_for_gap(&self_copy, token);
    if(Util_str_to_u64(token, value) == NULL) {
        return ParserMsg_new(self->offset, "expected number literal");
    }
    
    if(minus_flag) {
        *value = - (i64)*value;
    }

    *self = self_copy;
    
    return ParserMsg_new(self->offset, NULL);
}

ParserMsg Parser_parse_charliteral(inout Parser* self, out char c[3]) {
    ParserMsg msg = ParserMsg_new(self->offset, "expected char literal");

    Parser self_copy = *self;

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(self, "\'"),
        (void)NULL
    );

    c[0] = Parser_read(self);
    c[1] = '\0';
    switch(c[0]) {
        case '\0':
            return msg;
        case '\\':
            c[1] = Parser_read(self);
            switch(c[1]) {
                case '0':
                case 'n':
                case 'r':
                case '\\':
                case 't':
                case 'a':
                case '"':
                case '\'':
                    break;
                default:
                    return msg;
            }
            c[2] = '\0';
        default:
            break;
    }

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(self, "\'"),
        (void)NULL
    );

    *self = self_copy;

    return ParserMsg_new(self->offset, NULL);
}

ParserMsg Parser_parse_block(inout Parser* self, out Parser* parser) {
    return Parser_parse_block_helper(self, parser, "{", "}");
}

ParserMsg Parser_parse_paren(inout Parser* self, out Parser* parser) {
    return Parser_parse_block_helper(self, parser, "(", ")");
}

ParserMsg Parser_parse_index(inout Parser* self, out Parser* parser) {
    return Parser_parse_block_helper(self, parser, "[", "]");
}

ParserMsg ParserMsg_new(Offset offset, optional char* msg) {
    ParserMsg parser_msg;
    
    parser_msg.offset = offset;
    if(msg == NULL) {
        parser_msg.msg[0] = '\0';
    }else {
        strncpy(parser_msg.msg, msg, 256);
    }

    return parser_msg;
}

bool ParserMsg_is_success(ParserMsg self) {
    return self.msg[0] == '\0';
}

ParserMsg ParserMsg_from_sresult(SResult sresult, Offset offset) {
    if(SRESULT_IS_OK(sresult)) {
        return ParserMsg_new(offset, NULL);
    }

    return ParserMsg_new(offset, sresult.error);
}

