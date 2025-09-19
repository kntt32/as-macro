#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include "parser.h"
#include "util.h"

static struct { char expr; char code; } ESCAPE_SEQUENCES[] = {
    {'\\', '\\'}, {'\'', '\''}, {'\"', '\"'}, {'?', '?'}, {'a', '\a'}, {'b', '\b'},
    {'f', '\f'}, {'n', '\n'}, {'r', '\r'}, {'t', '\t'}, {'v', '\v'}, {'0', '\0'},
};

Parser Parser_new(optional in char* src, in char* path) {
    assert(path != NULL);

    Parser parser = {src, (src != NULL)?(strlen(src)):(0), Offset_new(path), NULL};
    Parser_skip_space(&parser);
    return parser;
}

Parser Parser_empty(Offset offset) {
    Parser parser = {NULL, 0, offset, NULL};
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
    printf(", parser_vars: ");
    if(self->parser_vars != NULL) {
        Vec_print(self->parser_vars, ParserVar_print_for_vec);
    }else {
        printf("null");
    }
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

static bool Parser_skip_parens_helper(inout Parser* self, out Parser* parser, in char* start, in char* end) {
    assert(self != NULL && parser != NULL && start != NULL && end != NULL);

    if(!Parser_skip_symbol(self, start)) {
        return false;
    }

    *parser = Parser_split(self, end);

    return true;
}


static bool Parser_skip_long_comment(inout Parser* self) {
    if(!Parser_skip_symbol(self, "/*")) {
        return false;
    }

    loop {
        if(Parser_skip_symbol(self, "*/")
            || Parser_is_empty(self)) {
            return true;
        }

        Parser_read(self);
    }
}

static bool Parser_skip_comment(inout Parser* self) {
    if(!Parser_skip_symbol(self, "//")) {
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
    return Parser_skip_keyword(&self_copy, keyword);
}

bool Parser_start_with_symbol(in Parser* self, in char* symbol) {
    assert(self != NULL && symbol != NULL);
    
    Parser self_copy = *self;
    return Parser_skip_symbol(&self_copy, symbol);
}

Parser Parser_split(inout Parser* self, in char* symbol) {
    assert(self != NULL && symbol != NULL);
    
    Parser parser = *self;
    u32 len = parser.len;
    parser.len = 0;

    while(!Parser_is_empty(self) && !Parser_skip_symbol(self, symbol)) {
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
        if(Parser_skip_symbol(&self_copy, symbol)) {
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

    if(Parser_skip_ident(self, token)
        || Parser_skip_number(self, &value)
        || Parser_skip_block(self, &parser)
        || Parser_skip_paren(self, &parser)
        || Parser_skip_index(self, &parser)) {
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

static ParserMsg Parser_parse_ident_without_skipspace(inout Parser* self, out char token[256]) {
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

    return ParserMsg_new(self->offset, NULL);
}

static ParserMsg Parser_parse_ident_parse_var(Parser parser, out char token[256]) {
    assert(parser.parser_vars != NULL);

    char ident[256];
    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser, ident), (void)NULL
    );

    if(!Parser_is_empty(&parser)) {
        return ParserMsg_new(parser.offset, "unexpected token");
    }
    for(u32 i=0; i<Vec_len(parser.parser_vars); i++) {
        ParserVar* parser_var = Vec_index(parser.parser_vars, i);
        if(strcmp(parser_var->name, ident) == 0) {
            strcpy(token, parser_var->value);
            return ParserMsg_new(parser.offset, NULL);
        }
    }

    char msg[256] = "\"";
    wrapped_strcat(msg, token, sizeof(msg));
    wrapped_strcat(msg, "\" is undefined", sizeof(msg));
    return ParserMsg_new(parser.offset, msg);
}

ParserMsg Parser_parse_ident(inout Parser* self, out char token[256]) {
    if(self->parser_vars == NULL) {
        PARSERMSG_UNWRAP(
            Parser_parse_ident_without_skipspace(self, token), (void)NULL
        );
    }else {
        Parser self_copy = *self;
        token[0] = '\0';
        char internal_token[256];

        loop {
            char c = Parser_look(&self_copy);
            if(c == '\0') {
                break;
            }else if(c == '$') {
                Parser_read(&self_copy);
                Parser paren_parser;
                PARSERMSG_UNWRAP(
                    Parser_parse_paren(&self_copy, &paren_parser), (void)NULL
                );
                PARSERMSG_UNWRAP(
                    Parser_parse_ident_parse_var(paren_parser, internal_token), (void)NULL
                );
            }else if(!ParserMsg_is_success(Parser_parse_ident_without_skipspace(&self_copy, internal_token))){
                break;
            }

            u32 token_len = strlen(token);
            strncat(token, internal_token, 256 - token_len - 1);
        }

        if(token[0] == '\0') {
            return ParserMsg_new(self->offset, "expected ident");
        }

        *self = self_copy;
    }

    Parser_skip_space(self);
    return ParserMsg_new(self->offset, NULL);
}

static bool Parser_skip_ident_without_skipspace(inout Parser* self, out char token[256]) {
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
        return false;
    }

    return true;
}

static bool Parser_skip_ident_parse_var(Parser parser, out char token[256]) {
    assert(parser.parser_vars != NULL);

    char ident[256];
    if(!Parser_skip_ident(&parser, ident)) {
        return false;
    }

    if(!Parser_is_empty(&parser)) {
        return false;
    }
    for(u32 i=0; i<Vec_len(parser.parser_vars); i++) {
        ParserVar* parser_var = Vec_index(parser.parser_vars, i);
        if(strcmp(parser_var->name, ident) == 0) {
            strcpy(token, parser_var->value);
            return true;
        }
    }

    return false;
}

bool Parser_skip_ident(inout Parser* self, out char token[256]) {
    if(self->parser_vars == NULL) {
        if(!Parser_skip_ident_without_skipspace(self, token)) {
            return false;
        }
    }else {
        Parser self_copy = *self;
        token[0] = '\0';
        char internal_token[256];

        loop {
            char c = Parser_look(&self_copy);
            if(c == '\0') {
                break;
            }else if(c == '$') {
                Parser_read(&self_copy);
                Parser paren_parser;
                if(!Parser_skip_paren(&self_copy, &paren_parser)
                    || !Parser_skip_ident_parse_var(paren_parser, internal_token)) {
                    return false;
                }
            }else if(!Parser_skip_ident_without_skipspace(&self_copy, internal_token)) {
                break;
            }

            u32 token_len = strlen(token);
            strncat(token, internal_token, 256 - token_len - 1);
        }

        if(token[0] == '\0') {
            return false;
        }

        *self = self_copy;
    }

    Parser_skip_space(self);
    return true;
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

bool Parser_skip_keyword(inout Parser* self, in char* keyword) {
    assert(self != NULL && keyword != NULL);

    Parser self_copy = *self;

    char token[256];
    if(!Parser_skip_ident(&self_copy, token) || strcmp(token, keyword) != 0) {
        return false;
    }

    *self = self_copy;
    Parser_skip_space(self);
    return true;
}

ParserMsg Parser_parse_symbol(inout Parser* self, in char* symbol) {
    assert(self != NULL && symbol != NULL);
    
    if(self->len < strlen(symbol) || strncmp(self->src, symbol, strlen(symbol)) != 0) {
        char msg[256] = "expected symbol \'";
        wrapped_strcat(msg, symbol, sizeof(msg));
        wrapped_strcat(msg, "\'", sizeof(msg));
        return ParserMsg_new(self->offset, msg);
    }
    
    for(u32 i=0; i<strlen(symbol); i++) {
        Parser_read(self);
    }

    Parser_skip_space(self);
    return ParserMsg_new(self->offset, NULL);
}

bool Parser_skip_symbol(inout Parser* self, in char* symbol) {
    assert(self != NULL && symbol != NULL);
    
    if(self->len < strlen(symbol) || strncmp(self->src, symbol, strlen(symbol)) != 0) {
        return false;
    }
    
    for(u32 i=0; i<strlen(symbol); i++) {
        Parser_read(self);
    }

    Parser_skip_space(self);
    return true;
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

bool Parser_skip_number(inout Parser* self, out u64* value) {
    assert(self != NULL && value != NULL);
    
    char* end = NULL;

    errno = 0;
    *value = strtoll(self->src, &end, 0);
    if(errno == ERANGE || end == self->src) {
        return false;
    }
    
    while(self->src != end) {
        Parser_read(self);
    }

    Parser_skip_space(self);
    return true;
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
                c = '\0';
                Vec_push(string, &c);
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

bool Parser_skip_block(inout Parser* self, out Parser* parser) {
    return Parser_skip_parens_helper(self, parser, "{", "}");
}

ParserMsg Parser_parse_paren(inout Parser* self, out Parser* parser) {
    return Parser_parse_parens_helper(self, parser, "(", ")");
}

bool Parser_skip_paren(inout Parser* self, out Parser* parser) {
    return Parser_skip_parens_helper(self, parser, "(", ")");
}

ParserMsg Parser_parse_index(inout Parser* self, out Parser* parser) {
    return Parser_parse_parens_helper(self, parser, "[", "]");
}

bool Parser_skip_index(inout Parser* self, out Parser* parser) {
    return Parser_skip_parens_helper(self, parser, "[", "]");
}

void Parser_set_parser_vars(inout Parser* self, optional in Vec* parser_vars) {
    assert(Vec_size(parser_vars) == sizeof(ParserVar));
    self->parser_vars = parser_vars;
}

ParserMsg ParserMsg_new(Offset offset, optional char* msg) {
    ParserMsg parser_msg;
    
    parser_msg.offset = offset;
    if(msg == NULL) {
        parser_msg.msg[0] = '\0';
    }else {
        wrapped_strcpy(parser_msg.msg, msg, sizeof(parser_msg.msg));
    }

    return parser_msg;
}

bool ParserMsg_is_success(ParserMsg self) {
    return self.msg[0] == '\0';
}

bool ParserMsg_is_success_ptr(in ParserMsg* self) {
    return self->msg[0] == '\0';
}

ParserMsg ParserMsg_from_sresult(SResult sresult, Offset offset) {
    if(SResult_is_success(sresult)) {
        return ParserMsg_new(offset, NULL);
    }

    return ParserMsg_new(offset, sresult.error);
}

bool ParserVar_cmp_value(in ParserVar* self, in ParserVar* other) {
    return strcmp(self->value, other->value) == 0;
}

bool ParserVar_cmp_value_for_vec(in void* left, in void* right) {
    return ParserVar_cmp_value(left, right);
}

void ParserVar_print(in ParserVar* self) {
    printf("ParserVar { name: %s, value: %s }", self->name, self->value);
}

void ParserVar_print_for_vec(in void* ptr) {
    ParserVar_print(ptr);
}


