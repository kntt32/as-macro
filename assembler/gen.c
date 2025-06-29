#include <stdio.h>
#include "types.h"
#include "register.h"
#include "parser.h"
#include "gen.h"

static Type TYPES[] = {
    {"void", Type_Integer, {}, 0, 1},
    {"bool", Type_Integer, {}, 1, 1},
    {"i8",  Type_Integer, {}, 1, 1},
    {"i16", Type_Integer, {}, 2, 2},
    {"i32", Type_Integer, {}, 4, 4},
    {"i64", Type_Integer, {}, 8, 8},
    {"f32", Type_Floating, {}, 4, 4},
    {"f64", Type_Floating, {}, 8, 8},
};

Data DATA_VOID = {
    {"void", Type_Integer, {}, 0, 1},
    {StorageType_imm, {.imm = {Imm_Value, {.value = 0}}}}
};

static void Type_parse_ref(inout Parser* parser, inout Type* type) {
    while(ParserMsg_is_success(Parser_parse_symbol(parser, "*"))) {
        Type* child = malloc(sizeof(Type));
        memcpy(child, type, sizeof(Type));
        
        strncat(type->name, "*", sizeof(type->name) - strlen(type->name) - 1);
        type->type = Type_Ptr;
        type->body.t_ptr = child;
        type->size = 8;
        type->align = 8;
    }
}

static ParserMsg Type_parse_struct_members(Parser parser, in Generator* generator, inout Type* type) {
    type->body.t_struct = Vec_new(sizeof(StructMember));

    while(!Parser_is_empty(&parser)) {
        StructMember struct_member;
        PARSERMSG_UNWRAP(
            StructMember_parse(&parser, generator, &type->align, &type->size, &struct_member),
            Vec_free_all(type->body.t_struct, StructMember_free_for_vec)
        );
        Vec_push(&type->body.t_struct, &struct_member);

        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, ","),
                Vec_free_all(type->body.t_struct, StructMember_free_for_vec)
            );
        }
    }

    type->size = (type->size + type->align - 1)/type->align*type->align;

    return SUCCESS_PARSER_MSG;
}

ParserMsg Type_parse_struct(inout Parser* parser, in Generator* generator, out Type* type) {
    Parser parser_copy = *parser;

    type->type = Type_Struct;
    if(!ParserMsg_is_success(Parser_parse_ident(&parser_copy, type->name))) {
        type->name[0] = '\0';
    }

    Parser block_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_block(&parser_copy, &block_parser),
        (void)NULL
    );

    type->size = 0;
    type->align = 1;
    PARSERMSG_UNWRAP(
        Type_parse_struct_members(block_parser, generator, type),
        (void)NULL
    );

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

static ParserMsg Type_parse_enum_members(Parser parser, out Type* type) {
    type->body.t_enum = Vec_new(sizeof(EnumMember));

    i32 value = 0;
    while(!Parser_is_empty(&parser)) {
        EnumMember enum_member;
        PARSERMSG_UNWRAP(
            EnumMember_parse(&parser, &value, &enum_member),
            Vec_free(type->body.t_enum)
        );
        Vec_push(&type->body.t_enum, &enum_member);

        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, ","),
                Vec_free(type->body.t_enum)
            );
        }
    }

    return SUCCESS_PARSER_MSG;
}

ParserMsg Type_parse_enum(inout Parser* parser, out Type* type) {
    Parser parser_copy = *parser;

    type->type = Type_Enum;
    if(!ParserMsg_is_success(Parser_parse_ident(&parser_copy, type->name))) {
        type->name[0] = '\0';
    }

    Parser block_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_block(&parser_copy, &block_parser),
        (void)NULL
    );

    type->size = 4;
    type->align = 4;
    PARSERMSG_UNWRAP(
        Type_parse_enum_members(block_parser, type),
        (void)NULL
    );

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

static ParserMsg Type_parse_array_get_args(Parser parser, in Generator* generator, out Type* type, out u32* len) {
    PARSERMSG_UNWRAP(
        Type_parse(&parser, generator, type),
        (void)NULL
    );

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser, ";"),
        Type_free(*type)
    );

    u64 value;
    PARSERMSG_UNWRAP(
        Parser_parse_number(&parser, &value),
        (void)NULL
    );
    if((i64)value <= 0) {
        ParserMsg msg = {parser.line, "array length must be natural number"};
        return msg;
    }
    *len = value;

    if(!Parser_is_empty(&parser)) {
        Type_free(*type);
        ParserMsg msg = {parser.line, "unexpected token"};
        return msg;
    }

    return SUCCESS_PARSER_MSG;
}

ParserMsg Type_parse_array(inout Parser* parser, in Generator* generator, out Type* type) {
    Parser parser_copy = *parser;

    Parser index_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_index(&parser_copy, &index_parser),
        (void)NULL
    );
    
    type->type = Type_Array;
    
    Type* child_type = malloc(sizeof(Type));
    type->body.t_array.type = child_type;
    UNWRAP_NULL(child_type);

    u32* len = &type->body.t_array.len;

    PARSERMSG_UNWRAP(
        Type_parse_array_get_args(index_parser, generator, child_type, len),
        free(child_type)
    );

    snprintf(type->name, 256, "[%.200s]", child_type->name);

    type->size = *len * child_type->size;
    type->align = child_type->align;

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

ParserMsg Type_parse(inout Parser* parser, in Generator* generator, out Type* type) {
    Parser parser_copy = *parser;

    char token[256];
    if(!ParserMsg_is_success(Parser_parse_ident(&parser_copy, token))) {
        PARSERMSG_UNWRAP(
            Type_parse_array(&parser_copy, generator, type),
            (void)NULL
        );
    }else if(strcmp(token, "struct") == 0) {
        PARSERMSG_UNWRAP(
            Type_parse_struct(&parser_copy, generator, type),
            (void)NULL
        );
    }else if(strcmp(token, "enum") == 0) {
        PARSERMSG_UNWRAP(
            Type_parse_enum(&parser_copy, type),
            (void)NULL
        );
    }else {
        SResult result = Generator_get_type(generator, token, type);
        if(!SRESULT_IS_OK(result)) {
            ParserMsg msg;
            msg.line = parser_copy.line;
            strcpy(msg.msg, result.error);
            return msg;
        }
    }

    Type_parse_ref(&parser_copy, type);

    *parser = parser_copy;

    return SUCCESS_PARSER_MSG;
}

bool Type_cmp(in Type* self, in Type* other) {
    if(strcmp(self->name, other->name) != 0
        || self->type != other->type
        || self->size != other->size
        || self->align != other->align) {
        return false;
    }

    switch(self->type) {
        case Type_Integer:
            break;
        case Type_Ptr:
            if(!Type_cmp(self->body.t_ptr, other->body.t_ptr)) {
                return false;
            }
            break;
        case Type_Array:
            if(!Type_cmp(self->body.t_array.type, other->body.t_array.type) || self->body.t_array.len != other->body.t_array.len) {
                return false;
            }
            break;
        case Type_Struct:
            if(!Vec_cmp(&self->body.t_struct, &other->body.t_struct, StructMember_cmp_for_vec)) {
                return false;
            }
            break;
        case Type_Enum:
            if(!Vec_cmp(&self->body.t_enum, &other->body.t_enum, EnumMember_cmp_for_vec)) {
                return false;
            }
            break;
        case Type_Floating:
            break;
    }
    return true;
}

Type Type_clone(in Type* self) {
    Type type = *self;
    switch(self->type) {
        case Type_Integer:
            break;
        case Type_Ptr:
            type.body.t_ptr = malloc(sizeof(Type));
            UNWRAP_NULL(type.body.t_ptr);
            *type.body.t_ptr = Type_clone(self->body.t_ptr);
            break;
        case Type_Array:
            type.body.t_array.type = malloc(sizeof(Type));
            UNWRAP_NULL(type.body.t_array.type);
            *type.body.t_array.type = Type_clone(self->body.t_array.type);
            type.body.t_array.len = self->body.t_array.len;
            break;
        case Type_Struct:
            type.body.t_struct = Vec_clone(&self->body.t_struct, StructMember_clone_for_vec);
            break;
        case Type_Enum:
            type.body.t_enum = Vec_clone(&self->body.t_enum, NULL);
            break;
        case Type_Floating:
            break;
    }
    return type;
}

void Type_print(in Type* self) {
    printf("Type { name: %s, type: %d, body: ", self->name, self->type);
    switch(self->type) {
        case Type_Integer:
            printf("none");
            break;
        case Type_Ptr:
            printf(".t_ptr: ");
            Type_print(self->body.t_ptr);
            break;
        case Type_Array:
            printf(".t_array: { type: ");
            Type_print(self->body.t_array.type);
            printf(", len: %u }", self->body.t_array.len);
            break;
        case Type_Struct:
            printf(".t_struct: ");
            Vec_print(&self->body.t_struct, StructMember_print_for_vec);
            break;
        case Type_Enum:
            printf(".t_enum: ");
            Vec_print(&self->body.t_enum, EnumMember_print_for_vec);
            break;
        case Type_Floating:
            printf("none");
            break;
    }
    printf(", size: %u, align: %lu }", self->size, self->align);
}

void Type_print_for_vec(void* ptr) {
    Type* self = ptr;
    Type_print(self);
}

void Type_free(Type self) {
    switch(self.type) {
        case Type_Integer:
            break;
        case Type_Ptr:
            Type_free(*self.body.t_ptr);
            free(self.body.t_ptr);
            break;
        case Type_Array:
            Type_free(*self.body.t_array.type);
            free(self.body.t_array.type);
            break;
        case Type_Struct:
            Vec_free_all(self.body.t_struct, StructMember_free_for_vec);
            break;
        case Type_Enum:
            Vec_free(self.body.t_enum);
            break;
        case Type_Floating:
            break;
    }
}

void Type_free_for_vec(void* ptr) {
    Type* self = ptr;
    Type_free(*self);
}

ParserMsg StructMember_parse(inout Parser* parser, in Generator* generator, inout u64* align, inout u32* size, out StructMember* struct_member) {
    Parser parser_copy = *parser;
    // name: Type
    
    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, struct_member->name),
        (void)NULL
    );
    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, ":"),
        (void)NULL
    );
    PARSERMSG_UNWRAP(
        Type_parse(&parser_copy, generator, &struct_member->type),
        (void)NULL
    );

    Type* type = &struct_member->type;
    if(*align < type->align) {
        *align = type->align;
    }
    struct_member->offset = (*size + type->align - 1)/type->align*type->align;
    *size = struct_member->offset + type->size;

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

StructMember StructMember_clone(in StructMember* self) {
    StructMember struct_member;

    strcpy(struct_member.name, self->name);
    struct_member.type = Type_clone(&self->type);
    struct_member.offset = self->offset;

    return struct_member;
}

void StructMember_clone_for_vec(out void* dst, in void* src) {
    StructMember* dst_ptr = dst;
    StructMember* src_ptr = src;

    *dst_ptr = StructMember_clone(src_ptr);
}

bool StructMember_cmp(in StructMember* self, in StructMember* other) {
    return strcmp(self->name, other->name)
        && self->offset == other->offset
        && Type_cmp(&self->type, &other->type);
}

bool StructMember_cmp_for_vec(in void* self, in void* other) {
    return StructMember_cmp(self, other);
}

void StructMember_print(in StructMember* self) {
    printf("StructMember { name: %s, type: ", self->name);
    Type_print(&self->type);
    printf(", offset: %u }", self->offset);
}

void StructMember_print_for_vec(in void* ptr) {
    StructMember_print(ptr);
}

void StructMember_free(StructMember self) {
    Type_free(self.type);
}

void StructMember_free_for_vec(void* ptr) {
    StructMember* member = ptr;
    StructMember_free(*member);
}

ParserMsg EnumMember_parse(inout Parser* parser, inout i32* value, out EnumMember* enum_member) {
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, enum_member->name),
        (void)NULL
    );
    if(ParserMsg_is_success(Parser_parse_symbol(&parser_copy, "="))) {
        u64 value_u64;
        PARSERMSG_UNWRAP(
            Parser_parse_number(&parser_copy, &value_u64),
            (void)NULL
        );
        *value = value_u64;
    }

    enum_member->value = *value;
    (*value) ++;

    *parser = parser_copy;

    return SUCCESS_PARSER_MSG;
}

bool EnumMember_cmp(in EnumMember* self, in EnumMember* other) {
    return self->value == other->value && strcmp(self->name, other->name);
}

bool EnumMember_cmp_for_vec(in void* self, in void* other) {
    return EnumMember_cmp(self, other);
}

void EnumMember_print(in EnumMember* self) {
    printf("EnumMember { name: %s, value: %d }", self->name, self->value);
}

void EnumMember_print_for_vec(in void* ptr) {
    EnumMember_print(ptr);
}

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

    return SRESULT_OK;
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
            ParserMsg msg = {parser->line, "expected register argument"};
            return msg;
        }
        mod_rm_type->body.r = sizes->reg;
    }else if(ParserMsg_is_success(Parser_parse_number(&parser_copy, &value))) {
        if(!(value < 8)) {
            ParserMsg msg = {parser_copy.line, "invlalid modrm encoding rule"};
            return msg;
        }
        mod_rm_type->type = ModRmType_Dight;
        mod_rm_type->body.dight = value;
    }

    if(sizes->regmem == 0) {
        ParserMsg msg = {parser->line, "expected reg/mem argument"};
        return msg;
    }
    mod_rm_type->memsize = sizes->regmem;

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

void ModRmType_print(in ModRmType* self) {
    printf("ModRmReg { type: ");
    switch(self->type) {
        case ModRmType_R:
            printf("ModRmType_R, body: .r: %u", self->body.r);
            break;
        case ModRmType_Dight:
            printf("ModRmType_Dight, body: .dight: %d", self->body.dight);
            break;
    }
    printf(", memsize: %u }", self->memsize);
}

static ParserMsg AsmEncodingElement_parse_imm_and_addreg(inout Parser* parser, out AsmEncodingElement* asm_encoding_element) {
    static struct { char str[5]; u8 imm_field; } IMMS[4] = {
        {"ib", 8}, {"iw", 16}, {"id", 32}, {"iq", 64}
    };
    static struct { char str[5]; u8 add_reg_field; } ADD_REGS[4] = {
        {"rb", 8}, {"rw", 16}, {"rd", 32}, {"rq", 64}
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

static SResult ModRmType_encode_rex_prefix_reg(in AsmArgs* args, inout u8* rex_prefix, inout bool* rex_prefix_needed_flag) {
    if(!args->reg_flag) {
        SResult result = {false, "expected register argument"};
        return result;
    }

    u8 modrmreg_code;
    switch(args->reg_type) {
        case AsmArgs_Reg_Reg:
            SRESULT_UNWRAP(
                Register_get_modrmreg_code(args->reg.reg, &modrmreg_code),
                (void)NULL
            );
            break;
        case AsmArgs_Reg_Xmm:
            TODO();
            break;
    }
    
    if(modrmreg_code & MODRMREG_REXR) {
        *rex_prefix |= REX_R;
        *rex_prefix_needed_flag = true;
    }
    if(modrmreg_code & MODRMREG_REX) {
        *rex_prefix_needed_flag = true;
    }

    return SRESULT_OK;
}

static SResult ModRmType_encode_rex_prefix_regmem(in ModRmType* self, in AsmArgs* args, inout u8* rex_prefix, inout bool* rex_prefix_needed_flag) {
    (void)self;
    (void)args;
    (void)rex_prefix;
    (void)rex_prefix_needed_flag;
    TODO();
    return SRESULT_OK;
}

SResult ModRmType_encode_rex_prefix(in ModRmType* self, in AsmArgs* args, inout u8* rex_prefix, inout bool* rex_prefix_needed_flag) {
    switch(self->type) {
        case ModRmType_R:
            SRESULT_UNWRAP(
                ModRmType_encode_rex_prefix_reg(args, rex_prefix, rex_prefix_needed_flag),
                (void)NULL
            );
            SRESULT_UNWRAP(
                ModRmType_encode_rex_prefix_regmem(self, args, rex_prefix, rex_prefix_needed_flag),
                (void)NULL
            )
            break;
        case ModRmType_Dight:
            SRESULT_UNWRAP(
                ModRmType_encode_rex_prefix_regmem(self, args, rex_prefix, rex_prefix_needed_flag),
                (void)NULL
            );
            break;
    }

    return SRESULT_OK;
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
            if(args->reg_type != AsmArgs_Reg_Reg) {
                SResult result = {false, "expecting integer register argument"};
                return result;
            }
            u8 addreg_code;
            SRESULT_UNWRAP(
                Register_get_addreg_code(args->reg.reg, &addreg_code),
                (void)NULL
            );
            if(addreg_code & ADDREG_REX) {
                *rex_prefix_need_flag = true;
            }
            if(addreg_code & ADDREG_REXB) {
                *rex_prefix |= REX_B;
                *rex_prefix_need_flag = true;
            }
            break;
    }
    
    return SRESULT_OK;
}

static SResult AsmEncodingElement_encode_value(in AsmEncodingElement* self, inout Generator* generator) {
    SRESULT_UNWRAP(
        Generator_append_binary(generator, "text", self->body.value),
        (void)NULL
    );
    return SRESULT_OK;
}

static SResult AsmEncodingElement_encode_imm(in AsmEncodingElement* self, in AsmArgs* args, inout Generator* generator) {
    if(!args->imm_flag) {
        SResult result = {false, "expected imm argument"};
        return result;
    }
    u64 value = 0;
    Imm* imm = &args->imm;
    switch(imm->type) {
        case Imm_Value:
            value = imm->body.value;
            break;
        case Imm_Label:
            value = 0;
            {
                Rel rel;
                strcpy(rel.label, imm->body.label);
                strcpy(rel.section_name, "text");
                rel.size = self->body.imm;
                SRESULT_UNWRAP(
                    Generator_binary_len(generator, "text", &rel.offset),
                    (void)NULL
                );
                SRESULT_UNWRAP(
                    Generator_new_rel(generator, rel),
                    (void)NULL
                );
            }
    }
    
    for(u32 i=0; i<self->body.imm/8; i++) {
        u8 byte = (value >> (8*i)) & 0xff;
        SRESULT_UNWRAP(
            Generator_append_binary(generator, "text", byte),
            (void)NULL
        );
    }
    return SRESULT_OK;
}

static SResult AsmEncodingElement_encode_addreg(in AsmArgs* args, inout Generator* generator) {
    if(!args->reg_flag || args->reg_type != AsmArgs_Reg_Reg) {
        SResult result = {false, "expected register argument"};
        return result;
    }

    u8 addreg_code;
    SRESULT_UNWRAP(
        Register_get_addreg_code(args->reg.reg, &addreg_code),
        (void)NULL
    );

    u8* byte;
    SRESULT_UNWRAP(
        Generator_last_binary(generator, "text", &byte),
        (void)NULL
    );
    *byte += addreg_code & ADDREG_ADDCODE;

    return SRESULT_OK;
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
            TODO();
            break;
        case AsmEncodingElement_AddReg:
            SRESULT_UNWRAP(
                AsmEncodingElement_encode_addreg(args, generator),
                (void)NULL
            );
            break;
    }

    return SRESULT_OK;
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
        asm_encoding->default_operand_size = 64;
    }else {
        PARSERMSG_UNWRAP(
            Parser_parse_keyword(&parser_copy, "double"),
            (void)NULL
        );
        asm_encoding->default_operand_size = 32;
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
    
    asm_encoding->operand_size = 32;
    asm_encoding->encoding_elements = elements;

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

static SResult AsmEncoding_encode_prefix_set_defaults(in AsmEncoding* self, inout bool* x66_prefix_needed_flag, inout u8* rex_prefix, inout bool* rex_prefix_needed_flag) {
    switch(self->default_operand_size) {
        case 32:
            switch(self->operand_size) {
                case 8:
                    break;
                case 16:
                    *x66_prefix_needed_flag = true;
                    break;
                case 32:
                    break;
                case 64:
                    *rex_prefix_needed_flag = true;
                    *rex_prefix |= REX_W;
                    break;
                default: PANIC("unreachable");
            }
            break;
        case 64:
            switch(self->operand_size) {
                case 16:
                    *x66_prefix_needed_flag = true;
                    break;
                case 32:
                    break;
                case 64:
                    *rex_prefix |= REX_W;
                    break;
                default: PANIC("unreachable");
            }
            break;
        default: PANIC("unreachable");
    }
    
    return SRESULT_OK;
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
            Generator_append_binary(generator, "text", x66_prefix),
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
            Generator_append_binary(generator, "text", rex_prefix),
            (void)NULL
        );
    }

    return SRESULT_OK;
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

    return SRESULT_OK;
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

void Index_print(in Index* self) {
    printf("Index { type: %d, body: ", self->type);
    switch(self->type) {
        case Index_None:
            printf("none");
            break;
        case Index_Reg:
            printf(".reg: ");
            Register_print(self->body.reg);
            break;
    }
    printf(", scale: %d }", self->scale);
}

void Disp_print(in Disp* self) {
    printf("Disp { type: %d, body: ", self->type);
    switch(self->type) {
        case Disp_None:
            printf("none");
            break;
        case Disp_Offset:
            printf(".offset: %d", self->body.offset);
            break;
        case Disp_Label:
            printf(".label: %s", self->body.label);
            break;
    }
    printf(" }");
}

bool Memory_cmp(in Memory* self, in Memory* other) {
    if(self->base != other->base) {
        return false;
    }

    if(self->index.type != other->index.type) {
        return false;
    }
    switch(self->index.type) {
        case Index_None:
            break;
        case Index_Reg:
            if(self->index.body.reg != other->index.body.reg) {
                return false;
            }
            break;
    }

    if(self->disp.type != other->disp.type) {
        return false;
    }
    switch(self->disp.type) {
        case Disp_None:
            break;
        case Disp_Offset:
            if(self->disp.body.offset != other->disp.body.offset) {
                return false;
            }
            break;
        case Disp_Label:
            if(strcmp(self->disp.body.label, other->disp.body.label) != 0) {
                return false;
            }
             break;
    }

    return true;
}

void Memory_print(in Memory* self) {
    printf("Memory { base: ");
    Register_print(self->base);
    printf(", index: ");
    Index_print(&self->index);
    printf(", disp: ");
    Disp_print(&self->disp);
    printf(" }");
}

void Imm_print(in Imm* self) {
    printf("Imm { type: %d, body: ", self->type);
    switch(self->type) {
        case Imm_Value:
            printf(".value: %lu", self->body.value);
            break;
        case Imm_Label:
            printf(".label: %s", self->body.label);
            break;
    }
    printf(" }");
}

bool Imm_cmp(in Imm* self, in Imm* other) {
    if(self->type != other->type) {
        return false;
    }

    switch(self->type) {
        case Imm_Value:
            if(self->body.value != other->body.value) {
                return false;
            }
            break;
        case Imm_Label:
            if(strcmp(self->body.label, other->body.label) != 0) {
                return false;
            }
            break;
    }

    return true;
}

ParserMsg Storage_parse(inout Parser* parser, in i32* rbp_offset, out Storage* storage) {
    if(ParserMsg_is_success(Register_parse(parser, &storage->body.reg))) {
        if(Register_is_integer(storage->body.reg)) {
            storage->type = StorageType_reg;
        }else if(Register_is_xmm(storage->body.reg)) {
            storage->type = StorageType_xmm;
            storage->body.xmm = storage->body.reg;
        }
    }else if(ParserMsg_is_success(Parser_parse_keyword(parser, "imm"))){
        storage->type = StorageType_imm;
        storage->body.imm.type = Imm_Value;
        storage->body.imm.body.value = 0;
    }else if(ParserMsg_is_success(Parser_parse_keyword(parser, "stack"))) {
        storage->type = StorageType_mem;
        Memory* mem = &storage->body.mem;
        mem->base = Rbp;
        mem->index.type = Index_None;
        mem->disp.type = Disp_Offset;
        mem->disp.body.offset = *rbp_offset;
    }else {
        ParserMsg msg = {parser->line, "expected storage"};
        return msg;
    }

    return SUCCESS_PARSER_MSG;
}

bool Storage_cmp(in Storage* self, in Storage* other) {
    if(self->type != other->type) {
        return false;
    }
    switch(self->type) {
        case StorageType_reg:
            return self->body.reg == other->body.reg;
        case StorageType_mem:
            return Memory_cmp(&self->body.mem, &other->body.mem);
        case StorageType_xmm:
            return self->body.xmm == other->body.xmm;
        case StorageType_imm:
            return Imm_cmp(&self->body.imm, &other->body.imm);
    }
    return false;
}

void Storage_print(in Storage* self) {
    printf("Storage { type: %d, body: ", self->type);
    switch(self->type) {
        case StorageType_reg:
            printf(".reg: ");
            Register_print(self->body.reg);
            break;
        case StorageType_mem:
            printf(".mem: ");
            Memory_print(&self->body.mem);
            break;
        case StorageType_xmm:
            printf(".xmm: ");
            Register_print(self->body.xmm);
            break;
        case StorageType_imm:
            printf(".imm: ");
            Imm_print(&self->body.imm);
            break;
    }
    printf(" }");
}

ParserMsg Data_parse(inout Parser* parser, in Generator* generator, inout i32* rbp_offset, out Data* data) {
    // Data @ Storage
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Type_parse(&parser_copy, generator, &data->type),
        (void)NULL
    );
    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "@"),
        Type_free(data->type)
    );
    *rbp_offset = (*rbp_offset + data->type.align - 1)/data->type.size*data->type.size;
    PARSERMSG_UNWRAP(
        Storage_parse(&parser_copy, rbp_offset, &data->storage),
        Type_free(data->type)
    );
    *rbp_offset = data->type.size;

    *parser = parser_copy;

    return SUCCESS_PARSER_MSG;
}

Data Data_from_register(Register reg) {
    if(Register_is_integer(reg)) {
        Data data = {
            {"i64", Type_Integer, {}, 8, 8},
            {StorageType_reg, {.reg = reg}}
        };
        return data;
    }else if(Register_is_xmm(reg)) {
        Data data = {
            {"f64", Type_Floating, {}, 8, 8},
            {StorageType_reg, {.reg = reg}}
        };
        return data;
    }
    
    PANIC("unreachable");
    Data uninit;
    return uninit;
}

Data Data_from_imm(u64 imm) {
    Data data = {
        {"i64", Type_Integer, {}, 8, 8},
        {StorageType_imm, {.imm = {Imm_Value, {.value = imm}}}}
    };
    return data;
}

Data Data_clone(in Data* self) {
    Data data;
    
    data.type = Type_clone(&self->type);
    data.storage = self->storage;
    
    return data;
}

void Data_print(in Data* self) {
    printf("Data { type: ");
    Type_print(&self->type);
    printf(", storage: ");
    Storage_print(&self->storage);
    printf(" }");
}

void Data_print_for_vec(in void* ptr) {
    Data_print(ptr);
}

void Data_free(Data self) {
    Type_free(self.type);
}

void Data_free_for_vec(inout void* ptr) {
    Data* data = ptr;
    Data_free(*data);
}

ParserMsg Variable_parse(inout Parser* parser, in Generator* generator, inout i32* rbp_offset, out Variable* variable) {
    // name: Data
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, variable->name),
        (void)NULL
    );
    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, ":"),
        (void)NULL
    );
    PARSERMSG_UNWRAP(
        Data_parse(&parser_copy, generator, rbp_offset, &variable->data),
        (void)NULL
    );

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

Variable Variable_clone(in Variable* self) {
    Variable variable;
    
    strcpy(variable.name, self->name);
    variable.data = Data_clone(&self->data);

    return variable;
}

void Variable_clone_for_vec(out void* dst, in void* src) {
    Variable* dst_ptr = dst;
    *dst_ptr = Variable_clone(src);
}

Type* Variable_get_type(in Variable* self) {
    return &self->data.type;
}

Storage* Variable_get_storage(in Variable* self) {
    return &self->data.storage;
}

void Variable_print(in Variable* self) {
    printf("Variable { name: %s, data: ", self->name);
    Data_print(&self->data);
    printf(" }");
}

void Variable_print_for_vec(in void* ptr) {
    Variable_print(ptr);
}

void Variable_free(Variable self) {
    Data_free(self.data);
}

void Variable_free_for_vec(inout void* ptr) {
    Variable* variable = ptr;
    Variable_free(*variable);
}

static ParserMsg Argument_parse_storage_bound(inout Parser* parser, inout Argument* argument) {
    Parser parser_copy = *parser;

    if(!Parser_start_with(parser, "imm") && ParserMsg_is_success(Storage_parse(&parser_copy, 0, &argument->storage.storage))) {
        argument->storage_type = Argument_Storage;
    }else {
        struct { char* keyword; bool* flag;} bounds[] = {
            {"reg", &argument->storage.trait.reg_flag},
            {"mem", &argument->storage.trait.mem_flag},
            {"imm", &argument->storage.trait.imm_flag}
        };
        
        for(u32 i=0; i<3; i++) {
            *bounds[i].flag = false;
        }

        do {
            bool flag = false;
            for(u32 i=0; i<LEN(bounds); i++) {
                if(ParserMsg_is_success(Parser_parse_keyword(&parser_copy, bounds[i].keyword))) {
                    *bounds[i].flag = true;
                    flag = true;
                    break;
                }
            }
            if(!flag) {
                ParserMsg msg = {parser->line, "expected storage bound"};
                return msg;
            }
        } while(ParserMsg_is_success(Parser_parse_symbol(&parser_copy, "+")));

        argument->storage_type = Argument_Trait;
    }

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

ParserMsg Argument_parse(inout Parser* parser, in Generator* generator, out Argument* argument) {
    // (in)(out) name : Type @ [reg | mem | xmm | imm]
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, argument->name),
        (void)NULL
    );
    
    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, ":"),
        (void)NULL
    );
    PARSERMSG_UNWRAP(
        Type_parse(&parser_copy, generator, &argument->type),
        (void)NULL
    );

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "@"),
        Argument_free(*argument)
    );
    PARSERMSG_UNWRAP(
        Argument_parse_storage_bound(&parser_copy, argument),
        Argument_free(*argument)
    );

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

bool Argument_cmp(in Argument* self, out Argument* other) {
    if(!Type_cmp(&self->type, &other->type)) {
        return false;
    }
    if(self->storage_type != other->storage_type) {
        return false;
    }
    switch(self->storage_type) {
        case Argument_Trait:    
            if(self->storage.trait.reg_flag != other->storage.trait.reg_flag
                || self->storage.trait.mem_flag != other->storage.trait.mem_flag) {
                return false;
            }
            break;
        case Argument_Storage:
            if(!Storage_cmp(&self->storage.storage, &other->storage.storage)) {
                return false;
            }
            break;
    }

    return strcmp(self->name, other->name);
}

bool Argument_cmp_for_vec(in void* self, in void* other) {
    return Argument_cmp(self, other);
}

Argument Argument_from(in Variable* variable) {
    Argument argument;
    strcpy(argument.name, variable->name);
    argument.type = Type_clone(&variable->data.type);
    argument.storage_type = Argument_Trait;
    switch(argument.type.type) {
        case Type_Floating:
            argument.storage.trait.reg_flag = false;
            argument.storage.trait.mem_flag = true;
            argument.storage.trait.imm_flag = true;
            argument.storage.trait.xmm_flag = true;
            break;
        default:
            argument.storage.trait.reg_flag = true;
            argument.storage.trait.mem_flag = true;
            argument.storage.trait.imm_flag = true;
            argument.storage.trait.xmm_flag = false;
            break;
    }

    return argument;
}

bool Argument_match_with(in Argument* self, in Data* data) {
    Storage storage = data->storage;
    switch(self->storage_type) {
        case Argument_Trait:
            if(!((self->storage.trait.reg_flag && storage.type == StorageType_reg)
                || (self->storage.trait.mem_flag && storage.type == StorageType_mem)
                || (self->storage.trait.imm_flag && storage.type == StorageType_imm)
                || (self->storage.trait.xmm_flag && storage.type == StorageType_xmm))) {
                return false;
            }
            if(self->storage.trait.imm_flag && (self->type.type == Type_Integer || self->type.type == Type_Floating) && self->type.type == data->type.type) {
                return true;
            }
            break;
        case Argument_Storage:
            if(!Storage_cmp(&storage, &self->storage.storage)) {
                return false;
            }
            break;
    }

    if(!Type_cmp(&self->type, &data->type)) {
        return false;
    }

   return true;
}

bool Argument_match_self(in Argument* self, in Argument* other) {
    if(!Type_cmp(&self->type, &other->type)) {
        return false;
    }

    switch(self->storage_type) {
        case Argument_Trait:
            switch(other->storage_type) {
            case Argument_Trait:
                if((!self->storage.trait.reg_flag && other->storage.trait.reg_flag)
                    || (!self->storage.trait.mem_flag && other->storage.trait.mem_flag)
                    || (!self->storage.trait.imm_flag && other->storage.trait.imm_flag)
                    || (!self->storage.trait.xmm_flag && other->storage.trait.xmm_flag)) {
                    return false;
                }
                break;
            case Argument_Storage:
            {
                Storage storage = other->storage.storage;
                if((!self->storage.trait.reg_flag && storage.type == StorageType_reg)
                    || (!self->storage.trait.mem_flag && storage.type == StorageType_mem)
                    || (!self->storage.trait.imm_flag && storage.type == StorageType_imm)
                    || (!self->storage.trait.xmm_flag && storage.type == StorageType_xmm)) {
                    return false;
                }
            }
                break;
            }
            break;
        case Argument_Storage:
            switch(other->storage_type) {
            case Argument_Trait:
                return false;
            case Argument_Storage:
                if(!Storage_cmp(&self->storage.storage, &other->storage.storage)) {
                    return false;
                }
                break;
            }
            break;
    }

    return true;
}

void Argument_print(in Argument* self) {
    printf("Argument { name: %s, type: ", self->name);
    Type_print(&self->type);
    printf(", storage_type: %d, storage: ", self->storage_type);
    switch(self->storage_type) {
        case Argument_Trait:
            printf(".trait: { reg_flag: %s, mem_flag: %s, imm_flag: %s }",
                BOOL_TO_STR(self->storage.trait.reg_flag),
                BOOL_TO_STR(self->storage.trait.mem_flag),
                BOOL_TO_STR(self->storage.trait.imm_flag));
            break;
        case Argument_Storage:
            printf(".storage: ");
            Storage_print(&self->storage.storage);
            break;
    }
    printf(" }");
}

void Argument_print_for_vec(in void* ptr) {
    Argument_print(ptr);
}

Argument Argument_clone(in Argument* self) {
    Argument argument;

    strcpy(argument.name, self->name);
    argument.type = Type_clone(&self->type);
    argument.storage_type = self->storage_type;
    switch(self->storage_type) {
        case Argument_Trait:
            argument.storage.trait = self->storage.trait;
            break;
        case Argument_Storage:
            argument.storage.storage = self->storage.storage;
            break;
    }

    return argument;
}

void Argument_clone_for_vec(in void* src, out void* dst) {
    Argument* src_ptr = src;
    *src_ptr = Argument_clone(dst);
}

void Argument_free(Argument self) {
    Type_free(self.type);
}

void Argument_free_for_vec(inout void* ptr) {
    Argument* argument = ptr;
    Argument_free(*argument);
}

static SResult AsmArgs_from_trait(in Data* data, in Argument* arg, inout AsmArgs* asm_args) {
    static SResult result = {false, "unsupported storage bound found"};

    bool reg_flag = arg->storage.trait.reg_flag;
    bool mem_flag = arg->storage.trait.mem_flag;
    bool imm_flag = arg->storage.trait.imm_flag;
    bool xmm_flag = arg->storage.trait.xmm_flag;

    switch(data->storage.type) {
        case StorageType_reg:
            if(reg_flag && mem_flag && !imm_flag && !xmm_flag) {
                asm_args->regmem_flag = true;
                asm_args->regmem_type = AsmArgs_Rm_Reg;
                asm_args->regmem.reg = data->storage.body.reg;
            }else if(reg_flag && !mem_flag && !imm_flag && !xmm_flag) {
                asm_args->reg_flag = true;
                asm_args->reg_type = AsmArgs_Reg_Reg;
                asm_args->reg.reg = data->storage.body.reg;
            }else {
                return result;
            }
            break;
        case StorageType_mem:
            asm_args->regmem_flag = true;
            asm_args->regmem_type = AsmArgs_Rm_Mem;
            if((reg_flag && mem_flag && !imm_flag && !xmm_flag)
                || (!reg_flag && mem_flag && !imm_flag && xmm_flag)
                || (!reg_flag && mem_flag && !imm_flag && !xmm_flag)) {
                asm_args->regmem_flag = true;
                asm_args->regmem_type = AsmArgs_Rm_Mem;
                asm_args->regmem.mem = data->storage.body.mem;
            }else {
                return result;
            }
            break;
        case StorageType_xmm:
            if(!reg_flag && mem_flag && !imm_flag && xmm_flag) {
                asm_args->regmem_flag = true;
                asm_args->regmem_type = AsmArgs_Rm_Xmm;
                asm_args->regmem.xmm = data->storage.body.xmm;
            }else if(!reg_flag && !mem_flag && !imm_flag && xmm_flag) {
                asm_args->reg_flag = true;
                asm_args->reg_type = AsmArgs_Reg_Xmm;
                asm_args->reg.xmm = data->storage.body.xmm;
            }else {
                return result;
            }
            break;
        case StorageType_imm:
            if(!reg_flag && !mem_flag && imm_flag && !xmm_flag) {
                asm_args->imm_flag = true;
                asm_args->imm = data->storage.body.imm;
            }else {
                return result;
            }
            break;
    }
    
    return SRESULT_OK;
}

SResult AsmArgs_from(in Vec* dataes, in Vec* arguments, out AsmArgs* asm_args) {
    asm_args->reg_flag = false;
    asm_args->regmem_flag = false;
    asm_args->imm_flag = false;

    for(u32 i=0; i<Vec_len(arguments); i++) {
        Argument* arg = Vec_index(arguments, i);
        Data* data = Vec_index(dataes, i);

        if(!Argument_match_with(arg, data)) {
            PANIC("mismatch with dataes and arguments");
        }
        switch(arg->storage_type) {
            case Argument_Trait:
                SRESULT_UNWRAP(
                    AsmArgs_from_trait(data, arg, asm_args),
                    (void)NULL
                );
                break;
            case Argument_Storage:
                break;
        }
    }

    return SRESULT_OK;
}

static ParserMsg Asmacro_parse_header_arguments(Parser parser, in Generator* generator, inout Vec* arguments) {
    while(!Parser_is_empty(&parser)) {
        Argument argument;
        PARSERMSG_UNWRAP(
            Argument_parse(&parser, generator, &argument),
            (void)NULL
        );
        Vec_push(arguments, &argument);

        if(!Parser_is_empty(&parser)) {
            if(!ParserMsg_is_success(Parser_parse_symbol(&parser, ","))) {
                ParserMsg msg = {parser.line, "expected symbol \",\""};
                return msg;
            }
        }
    }

    return SUCCESS_PARSER_MSG;
}

static ParserMsg Asmacro_parse_header(inout Parser* parser, in Generator* generator, out Asmacro* asmacro) {
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Parser_parse_keyword(&parser_copy, "as"),
        (void)NULL
    );
    
    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, asmacro->name),
        (void)NULL
    );
    
    Parser block_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser_copy, &block_parser),
        (void)NULL
    );
    asmacro->arguments = Vec_new(sizeof(Argument));
    PARSERMSG_UNWRAP(
        Asmacro_parse_header_arguments(block_parser, generator, &asmacro->arguments),
        Vec_free_all(asmacro->arguments, Argument_free_for_vec)
    );

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

static ParserMsg Asmacro_parse_proc(inout Parser* parser, out Asmacro* asmacro) {
    Parser block_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_block(parser, &block_parser),
        (void)NULL
    );
    
    asmacro->type = Asmacro_UserOperator;
    asmacro->body.user_operator.src = Parser_own(&block_parser, &asmacro->body.user_operator.parser);

    return SUCCESS_PARSER_MSG;
}

static ParserMsg Asmacro_parse_encoding(inout Parser* parser, out Asmacro* asmacro) {
    Parser parser_copy = *parser;

    Parser paren_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser_copy, &paren_parser),
        (void)NULL
    );

    AsmArgSize sizes;
    PARSERMSG_UNWRAP(
        ParserMsg_from_sresult(
            AsmArgSize_from(&asmacro->arguments, &sizes),
            parser->line
        ),
        (void)NULL
    );

    asmacro->type = Asmacro_AsmOperator;
    PARSERMSG_UNWRAP(
        AsmEncoding_parse(&paren_parser, &sizes, &asmacro->body.asm_operator),
        (void)NULL
    );

    if(Vec_len(&asmacro->arguments) != 0) {
        Argument* argument = Vec_index(&asmacro->arguments, 0);
        switch(argument->type.size) {
            case 1:
            case 2:
            case 4:
            case 8:
                break;
            default:
                AsmEncoding_free(asmacro->body.asm_operator);
                ParserMsg msg = {parser->line, "operand size must be 1, 2, 4 or 8 byte"};
                return msg;
        }
        asmacro->body.asm_operator.operand_size = argument->type.size * 8;
    }

    *parser = parser_copy;

    return SUCCESS_PARSER_MSG;
}

ParserMsg Asmacro_parse(inout Parser* parser, in Generator* generator, out Asmacro* asmacro) {
    // as name ( args ) { proc }
    // as name ( args ) : ( encoding )
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Asmacro_parse_header(&parser_copy, generator, asmacro),
        (void)NULL
    );

    if(ParserMsg_is_success(Parser_parse_symbol(&parser_copy, ":"))) {
        PARSERMSG_UNWRAP(
            Asmacro_parse_encoding(&parser_copy, asmacro),
            Vec_free_all(asmacro->arguments, Argument_free_for_vec)
        );
    }else {
        PARSERMSG_UNWRAP(
            Asmacro_parse_proc(&parser_copy, asmacro),
            Vec_free_all(asmacro->arguments, Argument_free_for_vec)
        );
    }

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

Asmacro Asmacro_new_fn_wrapper(in char* name, Vec arguments/* Vec<Variable> */) {
    Asmacro asmacro;

    strcpy(asmacro.name, name);
    
    asmacro.arguments = Vec_new(sizeof(Argument));
    for(u32 i=0; i<Vec_len(&arguments); i++) {
        Variable* variable = Vec_index(&arguments, i);
        Argument arg = Argument_from(variable);
        Vec_push(&asmacro.arguments, &arg);
    }
    
    asmacro.type = Asmacro_FnWrapper;
    asmacro.body.fn_wrapper = arguments;

    return asmacro;
}

SResult Asmacro_match_with(in Asmacro* self, in Vec* dataes) {
    if(Vec_len(&self->arguments) != Vec_len(dataes)) {
        SResult result = {false, "mismatching the number of arguments"};
        return result;
    }
    
    for(u32 i=0; i<Vec_len(&self->arguments); i++) {
        Argument* argument = Vec_index(&self->arguments, i);
        Data* data = Vec_index(dataes, i);
        if(!Argument_match_with(argument, data)) {
            SResult result = {false, ""};
            snprintf(result.error, 256, "mismatchig argument%u", i+1);
            return result;
        }
    }

    return SRESULT_OK;
}

bool Asmacro_cmp_signature(in Asmacro* self, in Asmacro* other) {
    return strcmp(self->name, other->name) == 0
        && Vec_cmp(&self->arguments, &other->arguments, Argument_cmp_for_vec);
}

void Asmacro_print(in Asmacro* self) {
    printf("Asmacro { name: %s, arguments: ", self->name);
    Vec_print(&self->arguments, Argument_print_for_vec);
    printf(", type: %d, body: ", self->type);

    switch(self->type) {
        case Asmacro_AsmOperator:
            printf(".asm_operator: ");
            AsmEncoding_print(&self->body.asm_operator);
            break;
        case Asmacro_UserOperator:
            printf(".user_operator: \"%s\"", self->body.user_operator.src);
            break;
        case Asmacro_FnWrapper:
            printf(".fn_wrapper: ");
            Vec_print(&self->body.fn_wrapper, Variable_print_for_vec);
            break;
    }
    
    printf(" }");
}

void Asmacro_print_for_vec(in void* ptr) {
    Asmacro_print(ptr);
}

Asmacro Asmacro_clone(in Asmacro* self) {
    Asmacro asmacro;

    strcpy(asmacro.name, self->name);
    asmacro.arguments = Vec_clone(&self->arguments, Argument_clone_for_vec);
    asmacro.type = self->type;

    switch(self->type) {
        case Asmacro_AsmOperator:
            asmacro.body.asm_operator = AsmEncoding_clone(&self->body.asm_operator);
            break;
        case Asmacro_UserOperator:
            asmacro.body.user_operator.src = Parser_own(&self->body.user_operator.parser, &asmacro.body.user_operator.parser);
            break;
        case Asmacro_FnWrapper:
            asmacro.body.fn_wrapper = Vec_clone(&self->body.fn_wrapper, Variable_clone_for_vec);
            break;
    }

    return asmacro;
}

void Asmacro_free(Asmacro self) {
    Vec_free_all(self.arguments, Argument_free_for_vec);
    switch(self.type) {
        case Asmacro_AsmOperator:
            AsmEncoding_free(self.body.asm_operator);
            break;
        case Asmacro_UserOperator:
            free(self.body.user_operator.src);
            break;
        case Asmacro_FnWrapper:
            Vec_free_all(self.body.fn_wrapper, Variable_free_for_vec);
            break;
    }
}

void Asmacro_free_for_vec(inout void* ptr) {
    Asmacro* asmacro = ptr;
    Asmacro_free(*asmacro);
}

SResult Section_new(in char* name, out Section* section) {
    if(!(strlen(name) < 256)) {
        SResult result = {false, "section name must be shorter than 256 bytes"};
        return result;
    }

    strcpy(section->name, name);
    section->binary = Vec_new(sizeof(u8));

    return SRESULT_OK;
}

void Section_print(in Section* self) {
    printf("Section { name: %s, binary: ", self->name);
    Vec_print(&self->binary, u8_print_for_vec);
    printf(" }");
}

void Section_print_for_vec(in void* ptr) {
    Section_print(ptr);
}

void Section_free(Section self) {
    Vec_free(self.binary);
}

void Section_free_for_vec(inout void* ptr) {
    Section* section = ptr;
    Section_free(*section);
}

void Label_print(in Label* self) {
    printf("Label { name: %s, public_flag: %s, section_name: %s, offset: %u }",
        self->name,
        BOOL_TO_STR(self->public_flag),
        self->section_name,
        self->offset
    );
}

void Label_print_for_vec(in void* ptr) {
    Label_print(ptr);
}

void Rel_print(in Rel* self) {
    printf("Rel { label: %s, section_name: %s, size: %u, offset: %u }",
        self->label,
        self->section_name,
        self->size,
        self->offset
    );
}

void Rel_print_for_vec(in void* ptr) {
    Rel_print(ptr);
}

Error Error_from_parsermsg(ParserMsg parser_msg) {
    Error error;
    error.line = parser_msg.line;
    strcpy(error.msg, parser_msg.msg);

    return error;
}

Error Error_from_sresult(u32 line, SResult result) {
    Error error;
    error.line = line;
    strcpy(error.msg, result.error);

    return error;
}

void Error_print(in Error* self) {
    printf("Error { line: %u, msg: %s }", self->line, self->msg);
}

void Error_print_for_vec(in void* ptr) {
    Error_print(ptr);
}

Generator Generator_new() {
    Generator generator = {
        Vec_from(TYPES, LEN(TYPES), sizeof(Type)),
        Vec_new(sizeof(Variable)),
        Vec_new(sizeof(Asmacro)),
        Vec_new(sizeof(Error)),
        Vec_new(sizeof(Section)),
        Vec_new(sizeof(Label)),
        Vec_new(sizeof(Rel))
    };
    return generator;
}

SResult Generator_add_type(inout Generator* self, Type type) {
    for(u32 i=0; i<Vec_len(&self->types); i++) {
        Type* ptr = Vec_index(&self->types, i);
        if(strcmp(ptr->name, type.name) == 0) {
            SResult result;
            result.ok_flag = false;
            snprintf(result.error, 256, "type \"%.10s\" has been already defined", type.name);
            Type_free(type);
            return result;
        }
    }

    Vec_push(&self->types, &type);

    return SRESULT_OK;
}

SResult Generator_get_type(in Generator* self, in char* name, out Type* type) {
    Type* ptr = NULL;
    for(u32 i=0; i<Vec_len(&self->types); i++) {
        ptr = Vec_index(&self->types, i);
        if(strcmp(ptr->name, name) == 0) {
            *type = Type_clone(ptr);
            return SRESULT_OK;
        }
    }

    SResult result;
    result.ok_flag = false;
    snprintf(result.error, 255, "type \"%.10s\" is undefined", name);

    return result;
}

SResult Generator_add_global_variable(inout Generator* self, Variable variable) {
    for(u32 i=0; i<Vec_len(&self->global_variables); i++) {
        Variable* ptr = Vec_index(&self->global_variables, i);
        if(strcmp(ptr->name, variable.name) == 0) {
            SResult result;
            result.ok_flag = false;
            snprintf(result.error, 256, "variable \"%.10s\" has been already defined", variable.name);
            Variable_free(variable);
            return result;
        }
    }

    Vec_push(&self->global_variables, &variable);

    return SRESULT_OK;
}

SResult Generator_add_asmacro(inout Generator* self, Asmacro asmacro) {
    for(u32 i=0; i<Vec_len(&self->asmacroes); i++) {
        Asmacro* ptr = Vec_index(&self->asmacroes, i);
        if(Asmacro_cmp_signature(ptr, &asmacro)) {
            SResult result;
            result.ok_flag = false;
            snprintf(result.error, 256, "asmacro \"%.10s\" has been already defined in same signature", asmacro.name);
            Asmacro_free(asmacro);
            return result;
        }
    }

    Vec_push(&self->asmacroes, &asmacro);
    return SRESULT_OK;
}

SResult Generator_get_asmacro(in Generator* self, in char* name, out Vec* asmacroes) {
    *asmacroes = Vec_new(sizeof(Asmacro));

    for(u32 i=0; i<Vec_len(&self->asmacroes); i++) {
        Asmacro* asmacro_ptr = Vec_index(&self->asmacroes, i);

        if(strcmp(asmacro_ptr->name, name) == 0) {
            Asmacro asmacro = Asmacro_clone(asmacro_ptr);
            Vec_push(asmacroes, &asmacro);
        }
    }

    if(Vec_len(asmacroes) == 0) {
        Vec_free(*asmacroes);
        SResult result;
        result.ok_flag = false;
        snprintf(result.error, 256, "asmacro \"%.10s\" is undefined", name);
        return result;
    }

    return SRESULT_OK;
}

void Generator_add_error(inout Generator* self, Error error) {
    Vec_push(&self->errors, &error);
}

SResult Generator_new_section(inout Generator* self, in char* name) {
    for(u32 i=0; i<Vec_len(&self->sections); i++) {
        Section* section = Vec_index(&self->sections, i);
        if(strcmp(section->name, name) == 0) {
            SResult result;
            result.ok_flag = true;
            snprintf(result.error, 256, "section \"%.10s\" has been already defined", name);
            return result;
        }
    }

    Section section;
    SRESULT_UNWRAP(
        Section_new(name, &section), (void)NULL
    );

    Vec_push(&self->sections, &section);

    return SRESULT_OK;
}

SResult Generator_new_label(inout Generator* self, Label label) {
    Vec_push(&self->labels, &label);
    return SRESULT_OK;
}

SResult Generator_new_rel(inout Generator* self, Rel rel) {
    Vec_push(&self->rels, &rel);
    return SRESULT_OK;
}

static SResult Generator_get_section(in Generator* self, in char* name, out Section** section) {
    for(u32 i=0; i<Vec_len(&self->sections); i++) {
        *section = Vec_index(&self->sections, i);
        if(strcmp((*section)->name, name) == 0) {
            return SRESULT_OK;
        }
    }

    SResult result;
    result.ok_flag = false;
    snprintf(result.error, 256, "section \"%.10s\" is undefined", name);
    return result;
}

SResult Generator_append_binary(inout Generator* self, in char* name, u8 byte) {
    Section* section;
    SRESULT_UNWRAP(
        Generator_get_section(self, name, &section),
        (void)NULL
    );
    Vec_push(&section->binary, &byte);
    return SRESULT_OK;
}

SResult Generator_last_binary(in Generator* self, in char* name, out u8** byte) {
    Section* section;
    SRESULT_UNWRAP(
        Generator_get_section(self, name, &section),
        (void)NULL
    );
    SRESULT_UNWRAP(
        Vec_last_ptr(&section->binary, (void**)byte),
        (void)NULL
    );
    return SRESULT_OK;
}

SResult Generator_binary_len(in Generator* self, in char* name, out u32* len) {
    Section* section;
    SRESULT_UNWRAP(
        Generator_get_section(self, name, &section),
        (void)NULL
    );

    *len = Vec_len(&section->binary);

    return SRESULT_OK;
}

void Generator_print(in Generator* self) {
    printf("Generator { types: ");
    Vec_print(&self->types, Type_print_for_vec);
    printf(", global_variables: ");
    Vec_print(&self->global_variables, Variable_print_for_vec);
    printf(", asmacroes: ");
    Vec_print(&self->asmacroes, Asmacro_print_for_vec);
    printf(", errors: ");
    Vec_print(&self->errors, Error_print_for_vec);
    printf(", sections: ");
    Vec_print(&self->sections, Section_print_for_vec);
    printf(", labels: ");
    Vec_print(&self->labels, Label_print_for_vec);
    printf(", rels: ");
    Vec_print(&self->rels, Rel_print_for_vec);
    printf(" }");
}

void Generator_free(Generator self) {
    Vec_free_all(self.types, Type_free_for_vec);
    Vec_free_all(self.global_variables, Variable_free_for_vec);
    Vec_free_all(self.asmacroes, Asmacro_free_for_vec);
    Vec_free(self.errors);
    Vec_free_all(self.sections, Section_free_for_vec);
    Vec_free(self.labels);
    Vec_free(self.rels);
}

void Generator_free_for_vec(inout void* ptr) {
    Generator* generator = ptr;
    Generator_free(*generator);
}

