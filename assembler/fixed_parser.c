#include <stdio.h>
#include "fixed_parser.h"
#include "util.h"

FixedParser FixedParser_new(in TokenField* field) {
    FixedParser this = {field, 0};
    return this;
}

static bool FixedParser_look(in FixedParser* self, optional out Token** token) {
    Vec* tokens = &self->token_field->tokens;
    
    if(Vec_len(tokens) <= self->cursor) {
        return false;
    }

    if(token != NULL) {
        *token = Vec_index(tokens, self->cursor);
    }

    return true;
}

static bool FixedParser_read(inout FixedParser* self, optional out Token** token) {
    if(!FixedParser_look(self, token)) {
        return false;
    }
    
    self->cursor ++;

    return true;
}

Offset FixedParser_offset(in FixedParser* self) {
    Token* token = NULL;

    Offset offset;
    if(FixedParser_look(self, &token)) {
        offset = token->offset;
    }else {
        offset = self->token_field->offset;
    }

    return offset;
}

FixedParserMsg FixedParser_parse_ident(inout FixedParser* self, out char token[256]) {
    Token* field_token = NULL;
    if(!FixedParser_look(self, &field_token)) {
        return FixedParserMsg_new(FixedParser_offset(self), "expected token");
    }

    if(field_token->type != TokenType_Keyword) {
        return FixedParserMsg_new(FixedParser_offset(self), "expected ident");
    }

    FixedParser_read(self, NULL);

    wrapped_strcpy(token, field_token->body.keyword, 256);

    return FixedParserMsg_new(FixedParser_offset(self), NULL);
}

FixedParserMsg FixedParser_parse_keyword(inout FixedParser* self, in char keyword[256]) {
    Token* field_token = NULL;
    if(!FixedParser_look(self, &field_token)) {
        return FixedParserMsg_new(FixedParser_offset(self), "expected token");
    }

    if(field_token->type != TokenType_Keyword) {
        return FixedParserMsg_new(FixedParser_offset(self), "expected keyword");
    }

    if(strncmp(field_token->body.keyword, keyword, 256) != 0) {
        char msg[256] = "expected keyword \"";
        wrapped_strcat(msg, keyword, 256);
        wrapped_strcat(msg, "\"", 256);
        return FixedParserMsg_new(FixedParser_offset(self), msg);
    }

    FixedParser_read(self, NULL);

    return FixedParserMsg_new(FixedParser_offset(self), NULL);
}

FixedParserMsg FixedParserMsg_new(Offset offset, optional char* msg) {
    FixedParserMsg parser_msg;
    
    parser_msg.offset = offset;
    if(msg == NULL) {
        parser_msg.msg[0] = '\0';
    }else {
        wrapped_strcpy(parser_msg.msg, msg, sizeof(parser_msg.msg));
    }

    return parser_msg;
}

bool FixedParserMsg_is_success(FixedParserMsg self) {
    return self.msg[0] == '\0';
}

bool FixedParserMsg_is_success_ptr(in FixedParserMsg* self) {
    return self->msg[0] == '\0';
}

FixedParserMsg FixedParserMsg_from_sresult(SResult sresult, Offset offset) {
    if(SResult_is_success(sresult)) {
        return FixedParserMsg_new(offset, NULL);
    }

    return FixedParserMsg_new(offset, sresult.error);
}

