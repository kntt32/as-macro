#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "parser.h"
#include "util.h"

ParserMsg SUCCESS_PARSER_MSG = {0, ""};

Parser Parser_new(in char* src) {
    Parser parser = {src, 1, strlen(src)};
    return parser;
}

static char Parser_read(inout Parser* self) {
    if(self->len == 0) {
        return '\0';
    }

    char c = self->src[0];

    if(c == '\n') {
        self->line ++;
    }

    self->src ++;
    self->len --;

    return c;
}

static void Parser_skip_space(inout Parser* self) {
    while(isspace(self->src[0]) && self->src[0] != '\0') {
        Parser_read(self);
    }
}

static bool Parser_is_gap(in Parser* self) {
    char c = self->src[0];
    return self->src[0] != '\0' && (isspace(c) || ispunct(c));
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

    char* ptr = NULL;
    if(ParserMsg_is_success(Parser_parse_stringliteral(self, &ptr))) {
        free(ptr);
        return true;
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
            ParserMsg msg = {self_copy.line, ""};
            sprintf(msg.msg, "expected symbol \"%s\"", end);
            return msg;
        }
    }

    parser->len = parser->len - self_copy.len - 1;
    *self = self_copy;

    return SUCCESS_PARSER_MSG;
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

ParserMsg Parser_split(inout Parser* self, in char* symbol, out Parser* parser) {
    Parser self_copy = *self;

    if(Parser_is_empty(self)) {
        *parser = *self;
        return SUCCESS_PARSER_MSG;
    }

    *parser = *self;
    while(!ParserMsg_is_success(Parser_parse_symbol(&self_copy, symbol))) {
        if(!Parser_skip(&self_copy)) {
            if(Parser_is_empty(&self_copy)) {
                break;
            }else {
                ParserMsg msg = {self_copy.line, "unknown token"};
                return msg;
            }
        }
        parser->len = self->len - self_copy.len;
    }

    *self = self_copy;

    return SUCCESS_PARSER_MSG;
}

ParserMsg Parser_parse_ident(inout Parser* self, out char token[256]) {
    Parser self_copy = *self;
    Parser_run_for_gap(&self_copy, token);
    u32 len = strlen(token);

    ParserMsg msg = {self_copy.line, "expected identifier"};
    
    if(len == 0) {
        return msg;
    }

    for(u32 i=0; i<len; i++) {
        if(!(isascii(token[i]) && (token[i] == '_' || isalpha(token[i]) || (i != 0 && isalnum(token[i]))))) {
            return msg;
        }
    }

    *self = self_copy;

    return SUCCESS_PARSER_MSG;
}

ParserMsg Parser_parse_keyword(inout Parser* self, in char* keyword) {
    Parser self_copy = *self;
    char token[256];
    Parser_run_for_gap(&self_copy, token);
    if(strcmp(token, keyword) != 0) {
        ParserMsg msg = {self_copy.line, "expected keyword"};
        return msg;
    }

    *self = self_copy;

    return SUCCESS_PARSER_MSG;
}

ParserMsg Parser_parse_symbol(inout Parser* self, in char* symbol) {
    Parser_skip_space(self);

    u32 symbol_len = strlen(symbol);
    if(strncmp(self->src, symbol, symbol_len) != 0) {
        ParserMsg msg = {self->line, "expected symbol"};
        return msg;
    }

    for(u32 i=0; i<symbol_len; i++) {
        Parser_read(self);
    }

    return SUCCESS_PARSER_MSG;
}

ParserMsg Parser_parse_number(inout Parser* self, out i64* value) {
    Parser self_copy = *self;
    
    char token[256];
    
    Parser_run_for_gap(&self_copy, token);
    
    if(Util_str_to_i64(token, value) == NULL) {
        ParserMsg msg = {self_copy.line, "expected number literal"};
        return msg;
    }

    *self = self_copy;
    
    return SUCCESS_PARSER_MSG;
}

ParserMsg Parser_parse_number_raw(inout Parser* self, out char value[256]) {
    Parser self_copy = *self;

    Parser_run_for_gap(&self_copy, value);

    if(!Util_is_number(value)) {
        ParserMsg msg = {self_copy.line, "expected number literal"};
        return msg;
    }

    *self = self_copy;
    
    return SUCCESS_PARSER_MSG;
}

static ParserMsg Parser_parse_stringliteral_withescapeflag(char c, u32 line) {
    ParserMsg msg = {line, "unknown escape sequence"};
            
    switch(c) {
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
    
    return SUCCESS_PARSER_MSG;
}

ParserMsg Parser_parse_stringliteral(inout Parser* self, out char** ptr) {
    Parser_skip_space(self);
    Parser self_copy = *self;

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&self_copy, "\""),
        (void)(NULL)
    );

    bool escape_flag = false;
    loop {
        char c = Parser_read(&self_copy);
        
        if(escape_flag) {
            escape_flag = true;
            PARSERMSG_UNWRAP(
                Parser_parse_stringliteral_withescapeflag(self_copy.line, c),
                (void)NULL
            );
        }

        switch(c) {
            case '\0':
                {
                    ParserMsg msg = {self_copy.len, "expected symbol \""};
                    return msg;
                }
            case '"':
                u32 len = self->len - self_copy.len - 2;
                *ptr = malloc(sizeof(char) * (len + 1));
                UNWRAP_NULL(*ptr);
                memcpy(*ptr, self->src + 1, len);
                (*ptr)[len] = '\0';
                *self = self_copy;
                return SUCCESS_PARSER_MSG;
            case '\\':
                escape_flag = true;
                break;
            default:
                break;
        }
    }

    PANIC("unreachable here");
    return SUCCESS_PARSER_MSG;
}

ParserMsg Parser_parse_charliteral(inout Parser* self, out char c[3]) {
    ParserMsg msg = {self->line, "invalid token"};

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

    return SUCCESS_PARSER_MSG;
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

bool ParserMsg_is_success(ParserMsg self) {
    return self.msg[0] == '\0';
}






