#include <stdio.h>
#include <assert.h>
#include "gen.h"

SResult AsmArgSize_from(in Vec* arguments, out AsmArgSize* sizes) {
    sizes->reg = 0;
    sizes->regmem = 0;

    for(u32 i=0; i<Vec_len(arguments); i++) {
        Argument* argument = Vec_index(arguments, i);
        switch(argument->storage_type) {
            case Argument_Trait:
                if(argument->storage.trait.mem_flag) {
                    sizes->regmem = argument->type.size;
                }else if(argument->storage.trait.reg_flag) {
                    sizes->reg = argument->type.size;
                }
                break;
            case Argument_Storage:
                break;
        }
    }

    return SResult_new(NULL);
}

void AsmArgSize_print(in AsmArgSize* self) {
    printf("AsmArgSize { reg: %u, regmem: %u }", self->reg, self->regmem);
}

ParserMsg ModRmType_parse(inout Parser* parser, in AsmArgSize* sizes, out ModRmType* mod_rm_type) {
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "/"),
        (void)NULL
    );

    u64 value;
    if(ParserMsg_is_success(Parser_parse_keyword(&parser_copy, "r"))) {
        mod_rm_type->type = ModRmType_R;
        if(sizes->reg == 0) {
            return ParserMsg_new(parser->offset, "expected register argument");
        }
        mod_rm_type->body.r = sizes->reg;
    }else if(ParserMsg_is_success(Parser_parse_number(&parser_copy, &value))) {
        if(!(value < 8)) {
            return ParserMsg_new(parser->offset, "invalid modrm encoding rule");
        }
        mod_rm_type->type = ModRmType_Digit;
        mod_rm_type->body.digit = value;
    }

    if(sizes->regmem == 0) {
        return ParserMsg_new(parser->offset, "expected reg/mem argument");
    }
    mod_rm_type->memsize = sizes->regmem;

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

void ModRmType_print(in ModRmType* self) {
    printf("ModRmReg { type: ");
    switch(self->type) {
        case ModRmType_R:
            printf("ModRmType_R, body: .r: %u", self->body.r);
            break;
        case ModRmType_Digit:
            printf("ModRmType_Dight, body: .digit: %d", self->body.digit);
            break;
    }
    printf(", memsize: %u }", self->memsize);
}

static ParserMsg AsmEncodingElement_parse_imm_and_addreg(inout Parser* parser, out AsmEncodingElement* asm_encoding_element) {
    static struct { char str[5]; u8 imm_field; } IMMS[4] = {
        {"ib", 1}, {"iw", 2}, {"id", 4}, {"iq", 8}
    };
    static struct { char str[5]; u8 add_reg_field; } ADD_REGS[4] = {
        {"rb", 1}, {"rw", 2}, {"rd", 4}, {"rq", 8}
    };

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
            return ParserMsg_new(parser->offset, NULL);
        }
    }
    for(u32 i=0; i<4; i++) {
        if(strcmp(token, ADD_REGS[i].str) == 0) {
            asm_encoding_element->type = AsmEncodingElement_AddReg;
            asm_encoding_element->body.add_reg = ADD_REGS[i].add_reg_field;
            
            *parser = parser_copy;
            return ParserMsg_new(parser->offset, NULL);
        }
    }

    return ParserMsg_new(parser->offset, "invalid asm encoding rule");
}

ParserMsg AsmEncodingElement_parse(inout Parser* parser, in AsmArgSize* sizes, out AsmEncodingElement* asm_encoding_element) {
    Parser parser_copy = *parser;

    u64 value;
    if(Parser_start_with_symbol(&parser_copy, "/")) {
        asm_encoding_element->type = AsmEncodingElement_ModRm;
        PARSERMSG_UNWRAP(
            ModRmType_parse(&parser_copy, sizes, &asm_encoding_element->body.mod_rm),
            (void)NULL
        );
    }else if(ParserMsg_is_success(Parser_parse_number(&parser_copy, &value))) {
        if(!(value < 256)) {
            return ParserMsg_new(parser_copy.offset, "invalid number");
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
    return ParserMsg_new(parser->offset, NULL);
}

static SResult ModRmType_encode_rex_prefix_reg(in ModRmType* self, in AsmArgs* args, inout u8* rex_prefix, inout bool* rex_prefix_needed_flag) {
    if(!args->reg_flag) {
        return SResult_new("expected register argument");
    }

    u8 modrmreg_code;
    switch(args->reg_type) {
        case AsmArgs_Reg_Reg:
            SRESULT_UNWRAP(
                Register_get_reg_code(args->reg.reg, &modrmreg_code),
                (void)NULL
            );
            break;
        case AsmArgs_Reg_Xmm:
            SRESULT_UNWRAP(
                Register_get_reg_code(args->reg.xmm, &modrmreg_code),
                (void)NULL
            );
            break;
    }
    
    if(modrmreg_code & REG_REXR) {
        *rex_prefix |= REX_R;
        *rex_prefix_needed_flag = true;
    }
    if(self->memsize == 1 && (modrmreg_code & REG_REX)) {
        *rex_prefix_needed_flag = true;
    }

    return SResult_new(NULL);
}

static SResult ModRmType_encode_rex_prefix_regmem_reg(in ModRmType* self, in AsmArgs* args, inout u8* rex_prefix, inout bool* rex_prefix_needed_flag) {
    assert(args != NULL && rex_prefix != NULL && rex_prefix_needed_flag != NULL);
    if(!args->regmem_flag || args->regmem_type != AsmArgs_Rm_Reg) {
        return SResult_new("unexpected encoding rule");
    }

    u8 code = 0;
    SRESULT_UNWRAP(
        Register_get_modrm_base_code(args->regmem.reg, &code),
        (void)NULL
    );

    if(code & MODRMBASE_REXB) {
        *rex_prefix |= REX_B;
        *rex_prefix_needed_flag = true;
    }

    if(self->memsize == 1 && (code & MODRMBASE_REX)) {
        *rex_prefix_needed_flag = true;
    }

    return SResult_new(NULL);
}

static SResult ModRmType_encode_rex_prefix_regmem_mem(in ModRmType* self, in AsmArgs* args, inout u8* rex_prefix, inout bool* rex_prefix_needed_flag) {
    if(!args->regmem_flag || args->regmem_type != AsmArgs_Rm_Mem) {
        return SResult_new("unexpected encoding rule");
    }
    Memory* memory = &args->regmem.mem;

    u8 code = 0;
    switch(memory->base) {
        case Rip:
            break;
        case Rsp:
            break;
        case R12:
            *rex_prefix_needed_flag = true;
            *rex_prefix |= REX_B;
            break;
        case Rbp:
            break;
        case R13:
            *rex_prefix_needed_flag = true;
            *rex_prefix |= REX_B;
            break;
        default:
            SRESULT_UNWRAP(
                Register_get_modrm_base_code(memory->base, &code),
                (void)NULL
            );
            if(code & MODRMBASE_REXB) {
                *rex_prefix_needed_flag = true;
                *rex_prefix |= REX_B;
            }
            if(self->memsize == 1 && (code & MODRMBASE_REX)) {
                *rex_prefix_needed_flag = true;
            }
            break;
    }

    return SResult_new(NULL);
}

static SResult ModRmType_encode_rex_prefix_regmem(in ModRmType* self, in AsmArgs* args, inout u8* rex_prefix, inout bool* rex_prefix_needed_flag) {
    assert(self != NULL && args != NULL && rex_prefix != NULL && rex_prefix_needed_flag != NULL);
    
    if(!args->regmem_flag) {
        return SResult_new("expected reg/mem argument");
    }

    switch(args->regmem_type) {
        case AsmArgs_Rm_Reg:
        case AsmArgs_Rm_Xmm:
            SRESULT_UNWRAP(
                ModRmType_encode_rex_prefix_regmem_reg(self, args, rex_prefix, rex_prefix_needed_flag),
                (void)NULL
            );
            break;
        case AsmArgs_Rm_Mem:
            SRESULT_UNWRAP(
                ModRmType_encode_rex_prefix_regmem_mem(self, args, rex_prefix, rex_prefix_needed_flag),
                (void)NULL
            );
            break;
    }

    return SResult_new(NULL);
}

SResult ModRmType_encode_rex_prefix(in ModRmType* self, in AsmArgs* args, inout u8* rex_prefix, inout bool* rex_prefix_needed_flag) {
    switch(self->type) {
        case ModRmType_R:
            SRESULT_UNWRAP(
                ModRmType_encode_rex_prefix_reg(self, args, rex_prefix, rex_prefix_needed_flag),
                (void)NULL
            );
            SRESULT_UNWRAP(
                ModRmType_encode_rex_prefix_regmem(self, args, rex_prefix, rex_prefix_needed_flag),
                (void)NULL
            );
            break;
        case ModRmType_Digit:
            SRESULT_UNWRAP(
                ModRmType_encode_rex_prefix_regmem(self, args, rex_prefix, rex_prefix_needed_flag),
                (void)NULL
            );
            break;
    }

    return SResult_new(NULL);
}

static SResult ModRmType_encode_reg(in ModRmType* self, in AsmArgs* args, inout u8* mod_rm) {
    u8 code = 0;
    switch(self->type) {
        case ModRmType_R:
            assert(args->reg_flag);
            if(args->reg_type == AsmArgs_Reg_Reg) {
                SRESULT_UNWRAP(
                    Register_get_reg_code(args->reg.reg, &code),
                    (void)NULL
                );
            }else {
                SRESULT_UNWRAP(
                    Register_get_reg_code(args->reg.xmm, &code),
                    (void)NULL
                );
            }
            break;
        case ModRmType_Digit:
            code = self->body.digit;
            break;
    }

    *mod_rm |= (code & 0x07) << 3;

    return SResult_new(NULL);
}

static SResult ModRmType_encode_mod_and_rm_mem(in AsmArgs* args, inout u8* mod, inout u8* code, inout bool* sib_flag, inout u8* sib, inout u32* disp_len) {
    assert(args->regmem_flag && args->regmem_type == AsmArgs_Rm_Mem);
    Memory* memory = &args->regmem.mem;
    u8 disp_size = Disp_size(&memory->disp);

    if(disp_size == 0) {
        *mod = 0b00;
        *disp_len = 0;
    }else if(disp_size == 1) {
        *mod = 0b01;
        *disp_len = 1;
    }else {
        *mod = 0b10;
        *disp_len = 4;
    }

    switch(memory->base) {
        case Rip:
            *mod = 0;
            *code = 0x05;
            *disp_len = 4;
            break;
        case Rsp:
        case R12:
            *code = 0b100;
            *sib_flag = true;
            *sib = 0x24;
            break;
        case Rbp:
        case R13:
            if(*mod == 0b00) {
                *mod = 0b01;
            }
            *code = 0b101;
            break;
        default:
            SRESULT_UNWRAP(
                Register_get_modrm_base_code(memory->base, code),
                (void)NULL
            );
            break;
    }

    return SResult_new(NULL);
}

static SResult ModRmType_encode_mod_and_rm(in AsmArgs* args, inout u8* mod_rm, inout bool* sib_flag, inout u8* sib, inout u32* disp_len) {
    assert(args->regmem_flag);

    u8 code = 0;
    u8 mod = 0;
    switch(args->regmem_type) {
        case AsmArgs_Rm_Reg:
            mod = 0b11;
            SRESULT_UNWRAP(
                Register_get_modrm_base_code(args->regmem.reg, &code),
                (void)NULL
            );
            break;
        case AsmArgs_Rm_Xmm:
            mod = 0b11;
            SRESULT_UNWRAP(
                Register_get_modrm_base_code(args->regmem.xmm, &code),
                (void)NULL
            );
            break;
        case AsmArgs_Rm_Mem:
            SRESULT_UNWRAP(
                ModRmType_encode_mod_and_rm_mem(args, &mod, &code, sib_flag, sib, disp_len),
                (void)NULL
            );
            break;
    }

    *mod_rm |= (mod & 0b11) << 6;
    *mod_rm |= code & 0x7;

    return SResult_new(NULL);
}

static SResult ModRmType_encode_disp(u32 len, in AsmArgs* args, inout Generator* generator) {
    if(len == 0) {
        return SResult_new(NULL);
    }
    assert(args->regmem_flag && args->regmem_type == AsmArgs_Rm_Mem);

    Disp* disp = &args->regmem.mem.disp;
    u64 disp_value = Disp_value(disp);
    Disp_set_label(disp, generator);

    for(u32 i=0; i<len; i++) {
        u8 byte = (disp_value >> (i*8)) & 0xff;
        SRESULT_UNWRAP(
            Generator_append_binary(generator, ".text", byte),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

SResult ModRmType_encode(in ModRmType* self, in AsmArgs* args, inout Generator* generator) {
    assert(self != NULL && args != NULL && generator != NULL && args->regmem_flag);

    u8 mod_rm = 0;
    SRESULT_UNWRAP(
        ModRmType_encode_reg(self, args, &mod_rm),
        (void)NULL
    );

    u32 disp_len = 0;
    bool sib_flag = false;
    u8 sib = 0;
    SRESULT_UNWRAP(
        ModRmType_encode_mod_and_rm(args, &mod_rm, &sib_flag, &sib, &disp_len),
        (void)NULL
    );

    SRESULT_UNWRAP(
        Generator_append_binary(generator, ".text", mod_rm),
        (void)NULL
    );

    if(sib_flag) {
        SRESULT_UNWRAP(
            Generator_append_binary(generator, ".text", sib),
            (void)NULL
        );
    }

    SRESULT_UNWRAP(
        ModRmType_encode_disp(disp_len, args, generator),
        (void)NULL
    );

    return SResult_new(NULL);
}

SResult AsmEncodingElement_encode_rexprefix(in AsmEncodingElement* self, in AsmArgs* args, inout u8* rex_prefix, inout bool* rex_prefix_need_flag) {
    switch(self->type) {
        case AsmEncodingElement_Value:
            break;
        case AsmEncodingElement_Imm:
            break;
        case AsmEncodingElement_ModRm:
            SRESULT_UNWRAP(
                ModRmType_encode_rex_prefix(&self->body.mod_rm, args, rex_prefix, rex_prefix_need_flag),
                (void)NULL
            );
            break;
        case AsmEncodingElement_AddReg:
            if(args->reg_flag && args->reg_type != AsmArgs_Reg_Reg) {
                return SResult_new("expecting integer register argument");
            }
            u8 addreg_code;
            SRESULT_UNWRAP(
                Register_get_addreg_code(args->reg.reg, &addreg_code),
                (void)NULL
            );
            if(self->body.add_reg == 1 && addreg_code & ADDREG_REX) {
                *rex_prefix_need_flag = true;
            }
            if(addreg_code & ADDREG_REXB) {
                *rex_prefix |= REX_B;
                *rex_prefix_need_flag = true;
            }
            break;
    }
    
    return SResult_new(NULL);
}

static SResult AsmEncodingElement_encode_value(in AsmEncodingElement* self, inout Generator* generator) {
    SRESULT_UNWRAP(
        Generator_append_binary(generator, ".text", self->body.value),
        (void)NULL
    );
    return SResult_new(NULL);
}

static SResult AsmEncodingElement_encode_imm(in AsmEncodingElement* self, in AsmArgs* args, inout Generator* generator) {
    if(!args->imm_flag) {
        return SResult_new("expected imm argument");
    }

    Imm* imm = &args->imm;
    switch(imm->type) {
        case Imm_Value:
        {
            u32 vec_len = Vec_len(&imm->body.value);
            for(u32 i=0; i<self->body.imm; i++) {
                u8 byte = (i < vec_len)?(*(u8*)Vec_index(&imm->body.value, i)):(0);
                SRESULT_UNWRAP(
                    Generator_append_binary(generator, ".text", byte),
                    (void)NULL
                );
            }
        }
            break;
        case Imm_Label:
            SRESULT_UNWRAP(
                Generator_append_rela(generator, ".text", imm->body.label, false),
                (void)NULL
            );
            for(u32 i=0; i<4; i++) {
                SRESULT_UNWRAP(
                    Generator_append_binary(generator, ".text", 0x00),
                    (void)NULL
                )
            }
            break;
    }
    
    return SResult_new(NULL);
}

static SResult AsmEncodingElement_encode_addreg(in AsmArgs* args, inout Generator* generator) {
    if(!args->reg_flag || args->reg_type != AsmArgs_Reg_Reg) {
        return SResult_new("expected register argument");
    }

    u8 addreg_code;
    SRESULT_UNWRAP(
        Register_get_addreg_code(args->reg.reg, &addreg_code),
        (void)NULL
    );

    u8* byte;
    SRESULT_UNWRAP(
        Generator_last_binary(generator, ".text", &byte),
        (void)NULL
    );
    *byte += addreg_code & ADDREG_ADDCODE;

    return SResult_new(NULL);
}

SResult AsmEncodingElement_encode(in AsmEncodingElement* self, in AsmArgs* args, inout Generator* generator) {
    switch(self->type) {
        case AsmEncodingElement_Value:
            SRESULT_UNWRAP(
                AsmEncodingElement_encode_value(self, generator),
                (void)NULL
            );
            break;
        case AsmEncodingElement_Imm:
            SRESULT_UNWRAP(
                AsmEncodingElement_encode_imm(self, args, generator),
                (void)NULL
            );
            break;
        case AsmEncodingElement_ModRm:
            SRESULT_UNWRAP(
                ModRmType_encode(&self->body.mod_rm, args, generator),
                (void)NULL
            );
            break;
        case AsmEncodingElement_AddReg:
            SRESULT_UNWRAP(
                AsmEncodingElement_encode_addreg(args, generator),
                (void)NULL
            );
            break;
    }

    return SResult_new(NULL);
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

ParserMsg AsmEncoding_parse(inout Parser* parser, in AsmArgSize* sizes, out AsmEncoding* asm_encoding) {
    Parser parser_copy = *parser;

    if(ParserMsg_is_success(Parser_parse_keyword(&parser_copy, "quad"))) {
        asm_encoding->default_operand_size = 8;
    }else {
        PARSERMSG_UNWRAP(
            Parser_parse_keyword(&parser_copy, "double"),
            (void)NULL
        );
        asm_encoding->default_operand_size = 4;
    }
    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, ":"),
        (void)NULL
    );
    
    Vec elements = Vec_new(sizeof(AsmEncodingElement));
    while(!Parser_is_empty(&parser_copy)) {
        AsmEncodingElement encoding_element;
        PARSERMSG_UNWRAP(
            AsmEncodingElement_parse(&parser_copy, sizes, &encoding_element),
            Vec_free(elements)
        );
        Vec_push(&elements, &encoding_element);

        if(!Parser_is_empty(&parser_copy)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser_copy, ","),
                Vec_free(elements)
            );
        }
    }
    
    asm_encoding->operand_size = 4;
    asm_encoding->encoding_elements = elements;

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

static SResult AsmEncoding_encode_prefix_set_defaults(in AsmEncoding* self, inout bool* x66_prefix_needed_flag, inout u8* rex_prefix, inout bool* rex_prefix_needed_flag) {
    switch(self->default_operand_size) {
        case 4:
            switch(self->operand_size) {
                case 1:
                    break;
                case 2:
                    *x66_prefix_needed_flag = true;
                    break;
                case 4:
                    break;
                case 8:
                    *rex_prefix_needed_flag = true;
                    *rex_prefix |= REX_W;
                    break;
                default: PANIC("unreachable");
            }
            break;
        case 8:
            switch(self->operand_size) {
                case 2:
                    *x66_prefix_needed_flag = true;
                    break;
                case 4:
                    break;
                case 8:
                    *rex_prefix |= REX_W;
                    break;
                default: PANIC("unreachable");
            }
            break;
        default: PANIC("unreachable");
    }
    
    return SResult_new(NULL);
}

static SResult AsmEncoding_encode_prefix(in AsmEncoding* self, in AsmArgs* args, inout Generator* generator) {
    u8 x66_prefix = 0x66;
    bool x66_prefix_needed_flag = false;
    u8 rex_prefix = 0x40;
    bool rex_prefix_needed_flag = false;


    SRESULT_UNWRAP(
        AsmEncoding_encode_prefix_set_defaults(self, &x66_prefix_needed_flag, &rex_prefix, &rex_prefix_needed_flag),
        (void)NULL
    );

    if(x66_prefix_needed_flag) {
        SRESULT_UNWRAP(
            Generator_append_binary(generator, ".text", x66_prefix),
            (void)NULL
        );
    }

    for(u32 i=0; i<Vec_len(&self->encoding_elements); i++) {
        AsmEncodingElement* element = Vec_index(&self->encoding_elements, i);
        SRESULT_UNWRAP(
            AsmEncodingElement_encode_rexprefix(element, args, &rex_prefix, &rex_prefix_needed_flag),
            (void)NULL
        );
    }
    if(rex_prefix_needed_flag) {
        SRESULT_UNWRAP(
            Generator_append_binary(generator, ".text", rex_prefix),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

SResult AsmEncoding_encode(in AsmEncoding* self, in AsmArgs* args, inout Generator* generator) {
    SRESULT_UNWRAP(
        AsmEncoding_encode_prefix(self, args, generator),
        (void)NULL
    );

    for(u32 i=0; i<Vec_len(&self->encoding_elements); i++) {
        AsmEncodingElement* element = Vec_index(&self->encoding_elements, i);
        SRESULT_UNWRAP(
            AsmEncodingElement_encode(element, args, generator),
            (void)NULL
        );
    }

    return Generator_addend_rela(generator, ".text");
}

void AsmEncoding_print(in AsmEncoding* self) {
    printf("AsmEncoding { encoding_elements: ");
    Vec_print(&self->encoding_elements, AsmEncodingElement_print_for_vec);
    printf(", default_operand_size: %u }", self->default_operand_size);
}

AsmEncoding AsmEncoding_clone(in AsmEncoding* self) {
    AsmEncoding asm_encoding;

    asm_encoding.encoding_elements = Vec_clone(&self->encoding_elements, NULL);
    asm_encoding.default_operand_size = self->default_operand_size;
    asm_encoding.operand_size = self->operand_size;

    return asm_encoding;
}

void AsmEncoding_free(AsmEncoding self) {
    Vec_free(self.encoding_elements);
}
