#pragma once

#include "types.h"
#include "vec.h"
#include "tokenizer.h"

typedef struct {
    char* src;
    u64 len;
    Offset offset;
    Vec* parser_vars;
} Parser;

typedef struct {
    Offset offset;
    char msg[256];
} ParserMsg;

typedef struct {
    char name[256];
    char value[256];
} ParserVar;

Parser Parser_new(optional in char* src, in char* path);
Parser Parser_empty(Offset offset);
void Parser_take(in Parser* self, out char string[256]);
void Parser_print(in Parser* self);
bool Parser_cmp(in Parser* self, in Parser* other);
char* Parser_path(in Parser* self);
void Parser_skip_space(inout Parser* self);
void Parser_skip(inout Parser* self);
bool Parser_is_empty(in Parser* self);
bool Parser_start_with(in Parser* self, in char* keyword);
bool Parser_start_with_symbol(in Parser* self, in char* symbol);
Parser Parser_split(inout Parser* self, in char* symbol);
Parser Parser_rsplit(inout Parser* self, in char* symbol);
ParserMsg Parser_parse_ident(inout Parser* self, out char token[256]);
bool Parser_skip_ident(inout Parser* self, out char token[256]);
ParserMsg Parser_parse_keyword(inout Parser* self, in char* keyword);
bool Parser_skip_keyword(inout Parser* self, in char* keyword);
ParserMsg Parser_parse_symbol(inout Parser* self, in char* symbol);
bool Parser_skip_symbol(inout Parser* self, in char* symbol);
ParserMsg Parser_parse_number(inout Parser* self, out u64* value);
bool Parser_skip_number(inout Parser* self, out u64* value);
ParserMsg Parser_parse_string(inout Parser* self, out Vec* string);
ParserMsg Parser_parse_char(inout Parser* self, out char* code);
ParserMsg Parser_parse_block(inout Parser* self, out Parser* parser);
bool Parser_skip_block(inout Parser* self, out Parser* parser);
ParserMsg Parser_parse_paren(inout Parser* self, out Parser* parser);
bool Parser_skip_paren(inout Parser* self, out Parser* parser);
ParserMsg Parser_parse_index(inout Parser* self, out Parser* parser);
bool Parser_skip_index(inout Parser* self, out Parser* parser);
void Parser_set_parser_vars(inout Parser* self, in Vec* parser_vars);

#define PARSERMSG_UNWRAP(parser_msg, catch_proc) {\
    ParserMsg msg = (parser_msg);\
    if(!ParserMsg_is_success_ptr(&msg)) {\
        catch_proc;\
        return msg;\
    }\
}

ParserMsg ParserMsg_new(Offset offset, optional char* msg);
bool ParserMsg_is_success(ParserMsg self);
bool ParserMsg_is_success_ptr(in ParserMsg* self);
ParserMsg ParserMsg_from_sresult(SResult sresult, Offset offset);

bool ParserVar_cmp_value(in ParserVar* self, in ParserVar* other);
bool ParserVar_cmp_value_for_vec(in void* left, in void* right);
void ParserVar_print(in ParserVar* self);
void ParserVar_print_for_vec(in void* ptr);


