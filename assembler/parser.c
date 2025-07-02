#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include "parser.h"

Offset Offset_new(in char* path) {
    assert(path != NULL);

    Offset offset;

    strncpy(offset.path, path, 255);
    offset.line = 1;
    offset.column = 1;

    return offset;
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

Parser Parser_new(in char* src, in char* path) {
    assert(src != NULL);
    assert(path != NULL);

    Parser parser = {src, strlen(src), Offset_new(path)};
    Parser_skip_space(&parser);
    return parser;
}

void Parser_print(in Parser* self) {
    assert(self != NULL);

    printf("Parser { src: \"");
    for(u32 i=0; i<self->len; i++) {
        printf("%c", self->src[i]);
    }
    printf("\", len: %lu, offset: ", self->len);
    Offset_print(&self->offset);
    printf(" }");
}

char* Parser_path(in Parser* self) {
    assert(self != NULL);
    return self->offset.path;
}

static char Parser_look(in Parser* self) {
    assert(self != NULL);

    if(self->len == 0) {
        return '\0';
    }

    return self->src[0];
}

static char Parser_read(inout Parser* self) {
    assert(self != NULL);
    
    char c = Parser_look(self);
    if(c == '\0') {
        return c;
    }

    Offset_seek_char(&self->offset, c);
    self->src ++;
    self->len --;

    return c;
}

static ParserMsg Parser_parse_parens_helper(inout Parser* self, out Parser* parser, in char* start, in char* end) {
    assert(self != NULL && parser != NULL && start != NULL && end != NULL);

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(self, start),
        (void)NULL
    );
    *parser = Parser_split(self, end);

    return ParserMsg_new(self->offset, NULL);
}

void Parser_skip_space(inout Parser* self) {
    assert(self != NULL);

    while(isspace((unsigned char)Parser_look(self))) {
        Parser_read(self);
    }
}

bool Parser_is_empty(in Parser* self) {
    assert(self != NULL);
    
    return self->len == 0;
}

bool Parser_start_with(in Parser* self, in char* keyword) {
    assert(self != NULL && keyword != NULL);
    
    Parser self_copy = *self;
    return ParserMsg_is_success(Parser_parse_keyword(&self_copy, keyword));
}

bool Parser_start_with_symbol(in Parser* self, in char* symbol) {
    assert(self != NULL && symbol != NULL);
    
    Parser self_copy = *self;
    return ParserMsg_is_success(Parser_parse_symbol(&self_copy, symbol));
}

Parser Parser_split(inout Parser* self, in char* symbol) {
    assert(self != NULL && symbol != NULL);
    
    Parser parser = *self;
    u32 len = parser.len;
    parser.len = 0;

    while(!Parser_is_empty(self) && !ParserMsg_is_success(Parser_parse_symbol(self, symbol))) {
        Parser_skip(self);
        parser.len = len - self->len;
    }

    return parser;
}

void Parser_skip(inout Parser* self) {
    assert(self != NULL);

    char token[256];
    u64 value;
    Parser parser;

    if(ParserMsg_is_success(Parser_parse_ident(self, token))
        || ParserMsg_is_success(Parser_parse_number(self, &value))
        || ParserMsg_is_success(Parser_parse_block(self, &parser))
        || ParserMsg_is_success(Parser_parse_paren(self, &parser))
        || ParserMsg_is_success(Parser_parse_index(self, &parser))) {
        return;
    }

    Parser_read(self);
}

ParserMsg Parser_parse_ident(inout Parser* self, out char token[256]) {
    assert(self != NULL && token != NULL);
    
    u32 len = 0;
    while(true) {
        char c = Parser_look(self);
        unsigned char u_c = (unsigned char)c;
        
        if(!isascii(u_c)) {
            break;
        }
        if(!(isalpha(u_c) || isdigit(u_c) || u_c == '_')) {
            break;
        }

        if(len < 256) token[len] = Parser_read(self);
        len ++;
    }

    if(len < 256) {
        token[len] = '\0';
    }else {
        token[255] = '\0';
    }
    if(len == 0) {
        return ParserMsg_new(self->offset, "expected ident");
    }

    Parser_skip_space(self);
    return ParserMsg_new(self->offset, NULL);
}

ParserMsg Parser_parse_keyword(inout Parser* self, in char* keyword) {
    assert(self != NULL && keyword != NULL);

    Parser self_copy = *self;

    char token[256];
    if(!ParserMsg_is_success(Parser_parse_ident(&self_copy, token)) || strcmp(token, keyword) != 0) {
        return ParserMsg_new(self->offset, "expected keyword");
    }

    *self = self_copy;
    Parser_skip_space(self);
    return ParserMsg_new(self->offset, NULL);
}

ParserMsg Parser_parse_symbol(inout Parser* self, in char* symbol) {
    assert(self != NULL && symbol != NULL);
    
    if(self->len < strlen(symbol) || strncmp(self->src, symbol, strlen(symbol)) != 0) {
        return ParserMsg_new(self->offset, "expected symbol");
    }
    
    for(u32 i=0; i<strlen(symbol); i++) {
        Parser_read(self);
    }

    Parser_skip_space(self);
    return ParserMsg_new(self->offset, NULL);
}

ParserMsg Parser_parse_number(inout Parser* self, out u64* value) {
    assert(self != NULL && value != NULL);
    
    char* end = NULL;

    errno = 0;
    *value = strtoll(self->src, &end, 0);
    if(errno == ERANGE || end == self->src) {
        return ParserMsg_new(self->offset, "expected number literal");
    }
    
    while(self->src != end) {
        Parser_read(self);
    }

    Parser_skip_space(self);
    return ParserMsg_new(self->offset, NULL);
}

ParserMsg Parser_parse_block(inout Parser* self, out Parser* parser) {
    return Parser_parse_parens_helper(self, parser, "{", "}");
}

ParserMsg Parser_parse_paren(inout Parser* self, out Parser* parser) {
    return Parser_parse_parens_helper(self, parser, "(", ")");
}

ParserMsg Parser_parse_index(inout Parser* self, out Parser* parser) {
    return Parser_parse_parens_helper(self, parser, "[", "]");
}

ParserMsg ParserMsg_new(Offset offset, optional char* msg) {
    ParserMsg parser_msg;
    
    parser_msg.offset = offset;
    if(msg == NULL) {
        parser_msg.msg[0] = '\0';
    }else {
        strncpy(parser_msg.msg, msg, 255);
    }

    return parser_msg;
}

bool ParserMsg_is_success(ParserMsg self) {
    return self.msg[0] == '\0';
}

ParserMsg ParserMsg_from_sresult(SResult sresult, Offset offset) {
    if(SResult_is_success(sresult)) {
        return ParserMsg_new(offset, NULL);
    }

    return ParserMsg_new(offset, sresult.error);
}

