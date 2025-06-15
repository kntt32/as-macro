#include <stdio.h>
#include "types.h"
#include "vec.h"
#include "asm.h"
#include "parser.h"

ParserMsg ModRmType_parse(inout Parser* parser, out ModRmType* mod_rm_type) {
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "/"),
        (void)NULL
    );

    i64 value;
    if(ParserMsg_is_success(Parser_parse_keyword(&parser_copy, "r"))) {
        mod_rm_type->type = ModRmType_R;
    }else if(ParserMsg_is_success(Parser_parse_number(&parser_copy, &value))) {
        if(!(0 <= value && value < 8)) {
            ParserMsg msg = {parser_copy.line, "invlalid modrm encoding rule"};
            return msg;
        }
        mod_rm_type->type = ModRmType_Dight;
        mod_rm_type->body.dight = value;
    }

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

void ModRmType_print(in ModRmType* self) {
    printf("ModRmReg { type: ");
    switch(self->type) {
        case ModRmType_R:
            printf("ModRmType_R, body: none");
            break;
        case ModRmType_Dight:
            printf("ModRmType_Dight, body: .dight: %d", self->body.dight);
            break;
    }
    printf(" }");
}

static ParserMsg AsmEncodingElement_parse_imm_and_addreg(inout Parser* parser, out AsmEncodingElement* asm_encoding_element) {
    static struct { char str[5]; u8 imm_field; } IMMS[4] = {{"ib", 8}, {"iw", 16}, {"id", 32}, {"iq", 64}};
    static struct { char str[5]; u8 add_reg_field; } ADD_REGS[4] = {{"rb", 8}, {"rw", 16}, {"rd", 32}, {"rq", 64}};

    Parser parser_copy = *parser;
    char token[256];
    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, token),
        (void)NULL
    );
        
    for(u32 i=0; i<4; i++) {
        if(strcmp(token, IMMS[i].str) == 0) {
            asm_encoding_element->type = AsmEncodingElement_Imm;
            asm_encoding_element->body.imm = IMMS[i].imm_field;
            
            *parser = parser_copy;
            return SUCCESS_PARSER_MSG;
        }
    }
    for(u32 i=0; i<4; i++) {
        if(strcmp(token, ADD_REGS[i].str) == 0) {
            asm_encoding_element->type = AsmEncodingElement_AddReg;
            asm_encoding_element->body.add_reg = ADD_REGS[i].add_reg_field;
            
            *parser = parser_copy;
            return SUCCESS_PARSER_MSG;
        }
    }

    ParserMsg msg = {parser_copy.line, "found invalid ssm encoding rule"};
    return msg;
}

ParserMsg AsmEncodingElement_parse(inout Parser* parser, out AsmEncodingElement* asm_encoding_element) {
    Parser parser_copy = *parser;

    i64 value;
    if(Parser_start_with_symbol(&parser_copy, "/")) {
        asm_encoding_element->type = AsmEncodingElement_ModRm;
        PARSERMSG_UNWRAP(
            ModRmType_parse(&parser_copy, &asm_encoding_element->body.mod_rm),
            (void)NULL
        );
    }else if(ParserMsg_is_success(Parser_parse_number(&parser_copy, &value))) {
        if(!(0 <= value && value < 256)) {
            ParserMsg msg = {parser_copy.line, "invalid number"};
            return msg;
        }
        asm_encoding_element->type = AsmEncodingElement_Value;
        asm_encoding_element->body.value = value;
    }else {
        PARSERMSG_UNWRAP(
            AsmEncodingElement_parse_imm_and_addreg(&parser_copy, asm_encoding_element),
            (void)NULL
        );
    }

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

void AsmEncodingElement_print(in AsmEncodingElement* self) {
    printf("AsmEncodingElement { type: %d, body: ", self->type);
    switch(self->type) {
        case AsmEncodingElement_Value:
            printf(".value: %u", self->body.value);
            break;
        case AsmEncodingElement_Imm:
            printf(".imm: %u", self->body.imm);
            break;
        case AsmEncodingElement_ModRm:
            printf(".mod_rm: ");
            ModRmType_print(&self->body.mod_rm);
            break;
        case AsmEncodingElement_AddReg:
            printf(".add_reg: %u", self->body.add_reg);
            break;
    }
    printf(" }");
}

void AsmEncodingElement_print_for_vec(in void* ptr) {
    AsmEncodingElement_print(ptr);
}

ParserMsg AsmEncoding_parse(Parser parser, in AsmEncoding* asm_encoding) {
    if(ParserMsg_is_success(Parser_parse_keyword(&parser, "quad"))) {
        asm_encoding->default_operand_size = 64;
    }else {
        PARSERMSG_UNWRAP(
            Parser_parse_keyword(&parser, "double"),
            (void)NULL
        );
        asm_encoding->default_operand_size = 32;
    }
    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser, ":"),
        (void)NULL
    );
    
    Vec elements = Vec_new(sizeof(AsmEncodingElement));
    while(!Parser_is_empty(&parser)) {
        AsmEncodingElement encoding_element;
        PARSERMSG_UNWRAP(
            AsmEncodingElement_parse(&parser, &encoding_element),
            Vec_free(elements)
        );
        Vec_push(&elements, &encoding_element);

        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, ","),
                (void)NULL
            );
        }
    }

    return SUCCESS_PARSER_MSG;
}

void AsmEncoding_print(in AsmEncoding* self) {
    printf("AsmEncoding { encoding_elements: ");
    Vec_print(&self->encoding_elements, AsmEncodingElement_print_for_vec);
    printf(", default_operand_size: %u }", self->default_operand_size);
}

void AsmEncoding_free(AsmEncoding self) {
    Vec_free(self.encoding_elements);
}

