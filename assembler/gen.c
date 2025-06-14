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
    printf(", size: %u, align: %llu }", self->size, self->align);
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

void EnumMember_print(in EnumMember* self) {
    printf("EnumMember { name: %s, value: %d }", self->name, self->value);
}

void EnumMember_print_for_vec(in void* ptr) {
    EnumMember_print(ptr);
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

void Variable_print(in Variable* self) {
    printf("Variable { name: %s, data: ", self->name);
    Data_print(&self->data);
    printf(" }");
}

void Variable_free(Variable self) {
    Data_free(self.data);
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
    Generator generator = { Vec_from(TYPES, LEN(TYPES), sizeof(Type)), Vec_new(sizeof(Error)) };
    return generator;
}

SResult Generator_add_type(inout Generator* self, Type type) {
    for(u32 i=0; i<Vec_len(&self->types); i++) {
        Type* ptr = Vec_index(&self->types, i);
        if(strcmp(ptr->name, type.name) == 0) {
            SResult result;
            result.ok_flag = false;
            snprintf(result.error, 256, "type \"%.10s\" has been already defined", type.name);
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

void Generator_add_error(inout Generator* self, Error error) {
    Vec_push(&self->errors, &error);
}

void Generator_print(in Generator* self) {
    printf("Generator { types: ");
    Vec_print(&self->types, Type_print_for_vec);
    printf(", errors: ");
    Vec_print(&self->errors, Error_print_for_vec);
    printf(" }");
}

void Generator_free(Generator self) {
    Vec_free_all(self.types, Type_free_for_vec);
    Vec_free(self.errors);
}

