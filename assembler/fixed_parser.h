#pragma once

#include "types.h"
#include "tokenizer.h"

typedef struct {
    TokenField* token_field;
    u64 cursor;
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
Offset FixedParser_offset(in FixedParser* self);
FixedParserMsg FixedParser_parse_ident(inout FixedParser* self, out char token[256]);
FixedParserMsg FixedParser_parse_keyword(inout FixedParser* self, in char keyword[256]);

FixedParserMsg FixedParserMsg_new(Offset offset, optional char* msg);
bool FixedParserMsg_is_success(FixedParserMsg self);
bool FixedParserMsg_is_success_ptr(in FixedParserMsg* self);
FixedParserMsg FixedParserMsg_from_sresult(SResult sresult, Offset offset);
