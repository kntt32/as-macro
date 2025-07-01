#pragma once

#include "types.h"

typedef struct {
    char path[256];
    u32 line;
    u32 column;
} Offset;

typedef struct {
    char* src;
    u64 len;
    Offset offset;
} Parser;

typedef struct {
    Offset offset;
    char msg[256];
} ParserMsg;

Offset Offset_new(in char* path);
void Offset_seek_char(inout Offset* self, char c);
void Offset_seek(inout Offset* self, char* token);
void Offset_print(in Offset* self);

Parser Parser_new(in char* src, in char* path);
void Parser_print(in Parser* self);
void Parser_skip_space(inout Parser* self);
void Parser_skip(inout Parser* self);
bool Parser_is_empty(in Parser* self);
bool Parser_start_with(in Parser* self, in char* keyword);
bool Parser_start_with_symbol(in Parser* self, in char* symbol);
Parser Parser_split(inout Parser* self, in char* symbol);
ParserMsg Parser_parse_ident(inout Parser* self, out char token[256]);
ParserMsg Parser_parse_keyword(inout Parser* self, in char* keyword);
ParserMsg Parser_parse_symbol(inout Parser* self, in char* symbol);
ParserMsg Parser_parse_number(inout Parser* self, out u64* value);
ParserMsg Parser_parse_block(inout Parser* self, out Parser* parser);
ParserMsg Parser_parse_paren(inout Parser* self, out Parser* parser);
ParserMsg Parser_parse_index(inout Parser* self, out Parser* parser);

#define PARSERMSG_UNWRAP(parser_msg, catch_proc) {\
    ParserMsg msg = (parser_msg);\
    if(!ParserMsg_is_success(msg)) {\
        catch_proc;\
        return msg;\
    }\
}

ParserMsg ParserMsg_new(Offset offset, optional char* msg);
bool ParserMsg_is_success(ParserMsg self);
ParserMsg ParserMsg_from_sresult(SResult sresult, Offset offset);

