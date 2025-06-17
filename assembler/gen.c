#include <stdio.h>
#include "types.h"
#include "register.h"
#include "parser.h"
#include "gen.h"

static Type TYPES[] = {
    {"u32", Type_Integer, {}, 4, 4},
    {"i32", Type_Integer, {}, 4, 4}
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

    i64 value;
    PARSERMSG_UNWRAP(
        Parser_parse_number(&parser, &value),
        (void)NULL
    );
    if(value <= 0) {
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
            UNWRAP_NULL(type.body.t_ptr);
            *type.body.t_array.type = Type_clone(self->body.t_array.type);
            type.body.t_array.len = self->body.t_array.len;
            break;
        case Type_Struct:
            type.body.t_struct = Vec_clone(&self->body.t_struct, StructMember_clone_for_vec);
            break;
        case Type_Enum:
            type.body.t_enum = Vec_clone(&self->body.t_enum, NULL);
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
        i64 value_i64;
        PARSERMSG_UNWRAP(
            Parser_parse_number(&parser_copy, &value_i64),
            (void)NULL
        );
        *value = value_i64;
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

void Memory_print(in Memory* self) {
    printf("Memory { ");
    if(self->base_flag) {
        printf("base: ");
        Register_print(self->base);
        printf(", ");
    }
    if(self->index_flag) {
        printf("index: ");
        Register_print(self->index);
        printf(", scale: %d, ", self->scale);
    }
    if(self->disp_flag) {
        printf("disp: %d, ", self->disp);
    }
    printf("}");
}

ParserMsg Storage_parse(inout Parser* parser, i32 rbp_offset, out Storage* storage) {
    if(ParserMsg_is_success(Register_parse(parser, &storage->body.reg))) {
        storage->type = StorageType_reg;
    }else if(ParserMsg_is_success(Parser_parse_keyword(parser, "stack"))) {
        storage->type = StorageType_mem;
        Memory* mem = &storage->body.mem;
        mem->base_flag = true;
        mem->base = Rbp;
        mem->index_flag = false;
        mem->disp_flag = rbp_offset != 0;
        mem->disp = rbp_offset;
    }else {
        ParserMsg msg = {parser->line, "expected storage"};
        return msg;
    }

    return SUCCESS_PARSER_MSG;
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
    }
    printf(" }");
}

void AsmArgs_print(in AsmArgs* self) {
    printf("AsmArgs { ");
    if(self->reg_flag) {
        printf("reg: ");
        Register_print(self->reg);
        printf(", ");
    }
    if(self->regmem_flag) {
        printf("regmem: ");
        Storage_print(&self->regmem);
        printf(", ");
    }
    if(self->imm_flag) {
        printf("imm: ");
        printf("%lu, ", self->imm);
    }
    printf("}");
}

void AsmArgs_print_for_vec(in void* ptr) {
    AsmArgs_print(ptr);
}

ParserMsg Data_parse(inout Parser* parser, in Generator* generator, i32 rbp_offset, out Data* data) {
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
    PARSERMSG_UNWRAP(
        Storage_parse(&parser_copy, rbp_offset, &data->storage),
        Type_free(data->type)
    );

    *parser = parser_copy;

    return SUCCESS_PARSER_MSG;
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

void Data_free(Data self) {
    Type_free(self.type);
}

ParserMsg Variable_parse(inout Parser* parser, in Generator* generator, i32 rbp_offset, out Variable* variable) {
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

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "@"),
        (void)NULL
    );

    argument->reg_flag = ParserMsg_is_success(Parser_parse_keyword(&parser_copy, "reg"));
    Parser_parse_symbol(&parser_copy, "/");
    argument->mem_flag = ParserMsg_is_success(Parser_parse_keyword(&parser_copy, "mem"));
    if(!argument->reg_flag && !argument->mem_flag) {
        ParserMsg msg = {parser_copy.line, "expected reg or mem"};
        return msg;
    }

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "."),
        (void)NULL
    );
    i64 size;
    PARSERMSG_UNWRAP(
        Parser_parse_number(&parser_copy, &size),
        (void)NULL
    );
    if(size <= 0) {
        ParserMsg msg = {parser_copy.line, "size must be greeter than zero"};
        return msg;
    }
    argument->size = size;

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

ParserMsg Argument_parse(inout Parser* parser, in Generator* generator, out Argument* argument) {
    // (in)(out) name (: Type) @ [reg | mem]size
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, argument->name),
        (void)NULL
    );

    argument->type_flag = ParserMsg_is_success(Parser_parse_symbol(&parser_copy, ":"));
    if(argument->type_flag) {
        PARSERMSG_UNWRAP(
            Type_parse(&parser_copy, generator, &argument->type),
            (void)NULL
        );
    }
    argument->reg_flag = false;
    argument->mem_flag = false;
    argument->size = 0;

    PARSERMSG_UNWRAP(
        Argument_parse_storage_bound(&parser_copy, argument),
        Argument_free(*argument)
    );

    if(argument->type_flag && argument->type.size != argument->size) {
        Argument_free(*argument);
        ParserMsg msg = {parser_copy.line, "conflicted argument size"};
        return msg;
    }

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
}

bool Argument_cmp(in Argument* self, out Argument* other) {
    if(self->type_flag && other->type_flag) {
        if(!Type_cmp(&self->type, &other->type)) {
            return false;
        }
    }
    return strcmp(self->name, other->name)
        && self->type_flag == other->type_flag
        && self->reg_flag == other->reg_flag
        && self->mem_flag == other->mem_flag
        && self->size == other->size;
}

bool Argument_cmp_for_vec(in void* self, in void* other) {
    return Argument_cmp(self, other);
}

void Argument_print(in Argument* self) {
    printf("Argument { name: %s", self->name);
    if(self->type_flag) {
        printf(", type: ");
        Type_print(&self->type);
    }
    printf(", reg_flag: %s, mem_flag: %s }", BOOL_TO_STR(self->reg_flag), BOOL_TO_STR(self->mem_flag));
}

void Argument_print_for_vec(in void* ptr) {
    Argument_print(ptr);
}

void Argument_free(Argument self) {
    if(self.type_flag) {
        Type_free(self.type);
    }
}

void Argument_free_for_vec(inout void* ptr) {
    Argument* argument = ptr;
    Argument_free(*argument);
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

    asmacro->type = Asmacro_AsmOperator;
    PARSERMSG_UNWRAP(
        AsmEncoding_parse(paren_parser, &asmacro->body.asm_operator),
        (void)NULL
    );

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
            (void)NULL
        );
    }else {
        PARSERMSG_UNWRAP(
            Asmacro_parse_proc(&parser_copy, asmacro),
            (void)NULL
        );
    }

    *parser = parser_copy;
    return SUCCESS_PARSER_MSG;
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
    }
    
    printf(" }");
}

void Asmacro_print_for_vec(in void* ptr) {
    Asmacro_print(ptr);
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
    printf("Label { name: %s, public_flag: %s, section_index: %d, offset: %lu }",
        self->name,
        BOOL_TO_STR(self->public_flag),
        self->section_index,
        self->offset
    );
}

void Label_print_for_vec(in void* ptr) {
    Label_print(ptr);
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

SResult Generator_append_binary(inout Generator* self, in char* name, u8 byte) {
    for(u32 i=0; i<Vec_len(&self->sections); i++) {
        Section* section = Vec_index(&self->sections, i);
        if(strcmp(section->name, name) == 0) {
            Vec_push(&section->binary, &byte);
            return SRESULT_OK;
        }
    }

    SResult result;
    result.ok_flag = false;
    snprintf(result.error, 256, "section \"%.10s\" is undefined", name);
    return result;
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
    printf(" }");
}

void Generator_free(Generator self) {
    Vec_free_all(self.types, Type_free_for_vec);
    Vec_free_all(self.global_variables, Variable_free_for_vec);
    Vec_free_all(self.asmacroes, Asmacro_free_for_vec);
    Vec_free(self.errors);
    Vec_free_all(self.sections, Section_free_for_vec);
    Vec_free(self.labels);
}

void Generator_free_for_vec(inout void* ptr) {
    Generator* generator = ptr;
    Generator_free(*generator);
}

