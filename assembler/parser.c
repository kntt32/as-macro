#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include "parser.h"

static struct { char expr; char code; } ESCAPE_SEQUENCES[] = {
    {'\\', '\\'}, {'\'', '\''}, {'\"', '\"'}, {'?', '?'}, {'a', '\a'}, {'b', '\b'},
    {'f', '\f'}, {'n', '\n'}, {'r', '\r'}, {'t', '\t'}, {'v', '\v'}, {'0', '\0'},
};

Offset Offset_new(in char* path) {
    assert(path != NULL);

    Offset offset;

    strncpy(offset.path, path, 255);
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

Parser Parser_new(in char* src, in char* path) {
    assert(src != NULL);
    assert(path != NULL);

    Parser parser = {src, strlen(src), Offset_new(path)};
    Parser_skip_space(&parser);
    return parser;
}

Parser Parser_empty(Offset offset) {
    Parser parser = {NULL, 0, offset};
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

bool Parser_cmp(in Parser* self, in Parser* other) {
    assert(self != NULL && other != NULL);
    
    if(!Offset_cmp(&self->offset, &other->offset)) {
        return false;
    }
    
    if(self->len != other->len) {
        return false;
    }
    
    if(self->len != 0 && strncmp(self->src, other->src, self->len) != 0) {
        return false;
    }
    
    return true;
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

static bool Parser_skip_long_comment(inout Parser* self) {
    if(!ParserMsg_is_success(
        Parser_parse_symbol(self, "/*")
    )) {
        return false;
    }

    loop {
        if(ParserMsg_is_success(Parser_parse_symbol(self, "*/"))
            || Parser_is_empty(self)) {
            return true;
        }

        Parser_read(self);
    }
}

static bool Parser_skip_comment(inout Parser* self) {
    if(!ParserMsg_is_success(
        Parser_parse_symbol(self, "//")
    )) {
        return false;
    }

    loop {
        char c = Parser_read(self);
        if(c == '\0' || c == '\n') {
            return true;
        }
    }
}

void Parser_skip_space(inout Parser* self) {
    assert(self != NULL);

    loop {
        if(isspace((unsigned char)Parser_look(self))) {
            Parser_read(self);
            continue;
        }
        if(Parser_skip_comment(self)) {
            continue;
        }
        if(Parser_skip_long_comment(self)) {
            continue;
        }
        break;
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

Parser Parser_rsplit(inout Parser* self, in char* symbol) {
    assert(self != NULL && symbol != NULL);

    u32 len = self->len;

    Parser left = *self;
    left.len = 0;
    Parser self_copy = *self;
    while(!Parser_is_empty(&self_copy)) {
        if(ParserMsg_is_success(Parser_parse_symbol(&self_copy, symbol))) {
            *self = self_copy;
            left.len = len - self->len - strlen(symbol);
        }else {
            Parser_skip(&self_copy);
        }
    }
    
    return left;
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

    Vec string;
    if(ParserMsg_is_success(Parser_parse_string(self, &string))) {
        Vec_free(string);
        return;
    }

    char code;
    if(ParserMsg_is_success(Parser_parse_char(self, &code))) {
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

ParserMsg Parser_parse_string(inout Parser* self, out Vec* string) {
    assert(self != NULL);
    assert(string != NULL);

    Parser self_copy = *self;

    if(Parser_read(&self_copy) != '\"') {
        return ParserMsg_new(self->offset, "expected symbol '\"'");
    }

    bool escape_sequence_flag = false;
    *string = Vec_new(sizeof(char));

    loop {
        char c = Parser_read(&self_copy);
        if(escape_sequence_flag) {
            escape_sequence_flag = false;

            for(u32 i=0; i<LEN(ESCAPE_SEQUENCES); i++) {
                if(c == ESCAPE_SEQUENCES[i].expr) {
                    Vec_push(string, &ESCAPE_SEQUENCES[i].code);
                    break;
                }
            }
        }else {
            if(c == '\"') {
                break;
            }
            if(c == '\0') {
                Vec_free(*string);
                return ParserMsg_new(self_copy.offset, "expected symbol '\"'");
            }
            if(c == '\\') {
                escape_sequence_flag = true;
                continue;
            }

            Vec_push(string, &c);
        }
    }

    *self = self_copy;
    Parser_skip_space(self);
    return ParserMsg_new(self->offset, NULL);
}

ParserMsg Parser_parse_char(inout Parser* self, out char* code) {
    assert(self != NULL);
    assert(code != NULL);

    Parser self_copy = *self;

    if(Parser_read(&self_copy) != '\'') {
        return ParserMsg_new(self->offset, "expected symbol \"\'\"");
    }

    *code = Parser_read(&self_copy);
    if(*code == '\\') {
        *code = Parser_read(&self_copy);
        for(i32 i=LEN(ESCAPE_SEQUENCES)-1; 0<=i; i--) {
            if(*code == ESCAPE_SEQUENCES[i].expr) {
                *code = ESCAPE_SEQUENCES[i].code;
                break;
            }

            if(i == 0) {
                return ParserMsg_new(self_copy.offset, "unexpected escape sequence");
            }
        }
    }else if(*code == '\0') {
        return ParserMsg_new(self_copy.offset, "expected charactor literal");
    }

    if(Parser_read(&self_copy) != '\'') {
        return ParserMsg_new(self_copy.offset, "expected symbol \"\'\"");
    }

    *self = self_copy;
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

