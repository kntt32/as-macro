#include <stdio.h>
#include <errno.h>
#include "fixed_parser.h"
#include "util.h"

static bool FixedParser_look(in FixedParser* self, optional out Token** token) {
    Vec* tokens = &self->token_field->tokens;
    
    if(Vec_len(tokens) <= self->cursor || self->len <= self->cursor) {
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

static void FixedParser_skip_whitespace(inout FixedParser* self) {
    loop {
        Token* token = NULL;
        if(!FixedParser_look(self, &token) || token->type != TokenType_Whitespace) {
            break;
        }
        FixedParser_read(self, NULL);
    }
}

static FixedParserMsg FixedParser_parse_parens_helper(inout FixedParser* self, char* start, char* end, out FixedParser* parser) {
    PARSERMSG_UNWRAP(
        FixedParser_parse_symbol(self, start), (void)NULL
    );

    *parser = FixedParser_split(self, end);

    return FixedParserMsg_new(FixedParser_offset(self), NULL);
}

FixedParser FixedParser_new(in TokenField* field) {
    FixedParser this = {field, 0, Vec_len(&field->tokens)};
    FixedParser_skip_whitespace(&this);
    return this;
}

void FixedParser_skip(inout FixedParser* self) {
    Token* token = NULL;
    if(!FixedParser_look(self, &token)) {
       return;
    }
    if(token->type != TokenType_Symbol || (strcmp(token->body.symbol, "(") != 0 && strcmp(token->body.symbol, "{") != 0 && strcmp(token->body.symbol, "[") != 0)) {
        FixedParser_read(self, NULL);
        return;
    }
    if(strcmp(token->body.symbol, "(") != 0) {
        FixedParser dummy;
        FixedParser_parse_paren(self, &dummy);
    }
    if(strcmp(token->body.symbol, "{") != 0) {
        FixedParser dummy;
        FixedParser_parse_block(self, &dummy);
    }
    if(strcmp(token->body.symbol, "[") != 0) {
        FixedParser dummy;
        FixedParser_parse_index(self, &dummy);
    }

    return;
}

void FixedParser_print(in FixedParser* self) {
    printf("FixedParser { token_field: %p, cursor: %lu, len: %lu }", self->token_field, self->cursor, self->len);
}

FixedParser FixedParser_split(inout FixedParser* self, in char* symbol) {
    FixedParser other = *self;
    Token* token = NULL;
    while(FixedParser_look(self, &token)) {
        if(token->type == TokenType_Symbol && strncmp(token->body.symbol, symbol, 256) == 0) {
            other.len = self->cursor;
            FixedParser_read(self, NULL);
            break;
        }
        FixedParser_skip(self);
    }

    FixedParser_skip_whitespace(self);

    return other;
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

    FixedParser_skip_whitespace(self);

    return FixedParserMsg_new(FixedParser_offset(self), NULL);
}

FixedParserMsg FixedParser_parse_keyword(inout FixedParser* self, in char* keyword) {
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
    FixedParser_skip_whitespace(self);

    return FixedParserMsg_new(FixedParser_offset(self), NULL);
}

FixedParserMsg FixedParser_parse_symbol(inout FixedParser* self, in char* symbol) {
    Token* field_token = NULL;
    if(!FixedParser_look(self, &field_token) || field_token->type != TokenType_Symbol || strncmp(field_token->body.symbol, symbol, 256) != 0) {
        char msg[256] = "";
        snprintf(msg, 255, "expected symbol \'%s\'", symbol);
        return FixedParserMsg_new(FixedParser_offset(self), msg);
    }

    FixedParser_read(self, NULL);
    FixedParser_skip_whitespace(self);

    return FixedParserMsg_new(FixedParser_offset(self), NULL);
}

FixedParserMsg FixedParser_parse_char(inout FixedParser* self, out char* code) {
    Token* field_token = NULL;
    if(!FixedParser_look(self, &field_token) || field_token->type != TokenType_Character) {
        return FixedParserMsg_new(FixedParser_offset(self), "expected character literal");
    }

    FixedParser_read(self, NULL);
    FixedParser_skip_whitespace(self);

    *code = field_token->body.character;

    return FixedParserMsg_new(FixedParser_offset(self), NULL);
}

FixedParserMsg FixedParser_parse_string(inout FixedParser* self, out Vec* string) {
    Token* field_token = NULL;
    if(!FixedParser_look(self, &field_token) || field_token->type != TokenType_String) {
        return FixedParserMsg_new(FixedParser_offset(self), "expected string literal");
    }

    FixedParser_read(self, NULL);
    FixedParser_skip_whitespace(self);

    *string = Vec_clone(&field_token->body.string, NULL);

    return FixedParserMsg_new(FixedParser_offset(self), NULL);
}

FixedParserMsg FixedParser_parse_number(inout FixedParser* self, out u64* value) {
    Token* field_token = NULL;
    if(!FixedParser_look(self, &field_token) || field_token->type != TokenType_Keyword) {
        return FixedParserMsg_new(FixedParser_offset(self), "expected number literal");
    }

    char* end = NULL;
    errno = 0;
    *value = strtoll(field_token->body.keyword, &end, 0);
    if(errno == ERANGE || end == field_token->body.keyword) {
        return FixedParserMsg_new(FixedParser_offset(self), "expected number literal");
    }

    FixedParser_read(self, &field_token);
    FixedParser_skip_whitespace(self);

    return FixedParserMsg_new(FixedParser_offset(self), NULL);
}

FixedParserMsg FixedParser_parse_paren(inout FixedParser* self, out FixedParser* parser) {
    return FixedParser_parse_parens_helper(self, "(", ")", parser);
}

FixedParserMsg FixedParser_parse_block(inout FixedParser* self, out FixedParser* parser) {
    return FixedParser_parse_parens_helper(self, "{", "}", parser);
}

FixedParserMsg FixedParser_parse_index(inout FixedParser* self, out FixedParser* parser) {
    return FixedParser_parse_parens_helper(self, "[", "]", parser);
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

