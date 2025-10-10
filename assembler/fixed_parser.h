#pragma once

#include "types.h"
#include "tokenizer.h"

typedef struct {
    TokenField* token_field;
    u64 cursor;
    u64 len;
} FixedParser;

typedef struct {
    Offset offset;
    char msg[256];
} FixedParserMsg;

typedef struct {
    char name[256];
    char value[256];
} FixedParserVar;

#define PARSERMSG_UNWRAP(parser_msg, catch_proc) {\
    FixedParserMsg msg = (parser_msg);\
    if(!FixedParserMsg_is_success_ptr(&msg)) {\
        catch_proc;\
        return msg;\
    }\
}

FixedParser FixedParser_new(in TokenField* field);
FixedParser FixedParser_split(inout FixedParser* self, in char* symbol);
Offset FixedParser_offset(in FixedParser* self);
FixedParserMsg FixedParser_parse_ident(inout FixedParser* self, out char token[256]);
FixedParserMsg FixedParser_parse_keyword(inout FixedParser* self, in char* keyword);
FixedParserMsg FixedParser_parse_symbol(inout FixedParser* self, in char* symbol);
FixedParserMsg FixedParser_parse_char(inout FixedParser* self, out char* code);
FixedParserMsg FixedParser_parse_string(inout FixedParser* self, out Vec* string);
FixedParserMsg FixedParser_parse_number(inout FixedParser* self, out u64* value);

FixedParserMsg FixedParserMsg_new(Offset offset, optional char* msg);
bool FixedParserMsg_is_success(FixedParserMsg self);
bool FixedParserMsg_is_success_ptr(in FixedParserMsg* self);
FixedParserMsg FixedParserMsg_from_sresult(SResult sresult, Offset offset);
