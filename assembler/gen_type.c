#include <stdio.h>
#include <assert.h>
#include "types.h"
#include "gen.h"

Vec Type_primitives(void) {
    Type primitives[] = {
        {"void", "", Type_Integer, {}, 0, 1},
        {"bool", "", Type_Integer, {}, 1, 1},
        {"char", "", Type_Integer, {}, 1, 1},
        {"i8",  "", Type_Integer, {}, 1, 1},
        {"i16", "", Type_Integer, {}, 2, 2},
        {"i32", "", Type_Integer, {}, 4, 4},
        {"i64", "", Type_Integer, {}, 8, 8},
        {"f32", "", Type_Floating, {}, 4, 4},
        {"f64", "", Type_Floating, {}, 8, 8},
        {"b8", "", Type_Integer, {}, 1, 1},
        {"b16", "", Type_Integer, {}, 2, 2},
        {"b32", "", Type_Integer, {}, 4, 4},
        {"b64", "", Type_Integer, {}, 8, 8},
        {"b", "", Type_Integer, {}, 1, 1},
        {"t", "", Type_Integer, {}, 1, 1},
    };
    Vec types = Vec_from(primitives, LEN(primitives), sizeof(Type));

    return types;
}

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

    return ParserMsg_new(parser.offset, NULL);
}

ParserMsg Type_parse_struct(inout Parser* parser, in Generator* generator, out Type* type) {
    Parser parser_copy = *parser;

    type->type = Type_Struct;
    if(!ParserMsg_is_success(Parser_parse_ident(&parser_copy, type->name))) {
        type->name[0] = '\0';
    }
    type->valid_path[0] = '\0';

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
    return ParserMsg_new(parser->offset, NULL);
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

    return ParserMsg_new(parser.offset, NULL);
}

ParserMsg Type_parse_enum(inout Parser* parser, out Type* type) {
    Parser parser_copy = *parser;

    type->type = Type_Enum;
    if(!ParserMsg_is_success(Parser_parse_ident(&parser_copy, type->name))) {
        type->name[0] = '\0';
    }
    type->valid_path[0] = '\0';

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
    return ParserMsg_new(parser->offset, NULL);
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
        return ParserMsg_new(parser.offset, "array length must be natural number");
    }
    *len = value;

    if(!Parser_is_empty(&parser)) {
        Type_free(*type);
        return ParserMsg_new(parser.offset, "unexpected token");
    }

    return ParserMsg_new(parser.offset, NULL);
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

    snprintf(type->name, 256, "[%.200s; %u]", child_type->name, *len);
    type->valid_path[0] = '\0';

    type->size = *len * child_type->size;
    type->align = child_type->align;

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Type_parse_lazyptr(inout Parser* parser, out Type* type) {
    assert(parser != NULL && type != NULL);

    Parser parser_copy = *parser;
    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, type->name),
        (void)NULL
    );
    type->valid_path[0] = '\0';

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "*"),
        (void)NULL
    );

    strcpy(type->body.t_lazy_ptr, type->name);
    strncat(type->name, "*", 255 - strlen(type->name) - 1);

    type->type = Type_LazyPtr;
    type->size = 8;
    type->align = 8;

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Type_parse_fn(inout Parser* parser, in Generator* generator, out Type* type) {
    // fn(..)

    Parser parser_copy = *parser;

    Parser paren_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser_copy, &paren_parser), (void)NULL
    );

    Vec arguments = Vec_new(sizeof(Data));
    while(!Parser_is_empty(&paren_parser)) {
        i32 dummy = 0;
        Data argument;
        PARSERMSG_UNWRAP(
            Data_parse(&paren_parser, generator, &dummy, &argument), Vec_free_all(arguments, Data_free_for_vec)
        );
        Vec_push(&arguments, &argument);

        if(!Parser_is_empty(&paren_parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&paren_parser, ","), Vec_free_all(arguments, Data_free_for_vec)
            );
        }
    }

    type->name[0] = '\0';
    type->valid_path[0] = '\0';
    type->type = Type_Fn;
    type->body.t_fn = arguments;
    type->size = 0;
    type->align = 0x10;

    *parser = parser_copy;

    return ParserMsg_new(parser->offset, NULL);
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
    }else if(strcmp(token, "fn") == 0) {
        PARSERMSG_UNWRAP(
            Type_parse_fn(&parser_copy, generator, type),
            (void)NULL
        );
    }else if(!SResult_is_success(
        Generator_get_type(generator, token, type)
    )) {
        parser_copy = *parser;

        PARSERMSG_UNWRAP(
            Type_parse_lazyptr(&parser_copy, type),
            (void)NULL
        );
    }

    if(type->valid_path[0] != '\0' && strcmp(Parser_path(&parser_copy), type->valid_path) != 0) {
        return ParserMsg_new(parser->offset, "out of namespace");
    }

    Type_parse_ref(&parser_copy, type);

    *parser = parser_copy;

    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Type_get_enum_member(Type self, Parser parser, out Data* data) {
    if(self.type != Type_Enum) {
        Type_free(self);
        return ParserMsg_new(parser.offset, "expected enum");
    }
    for(u32 i=0; i<Vec_len(&self.body.t_enum); i++) {
        EnumMember* member = Vec_index(&self.body.t_enum, i);
        if(ParserMsg_is_success(Parser_parse_keyword(&parser, member->name))) {
            if(!Parser_is_empty(&parser)) {
                break;
            }

            Vec bin = Vec_new(sizeof(u8));
            for(u32 k=0; k<4; k++) {
                u8 byte = (member->value >> (k*8)) & 0xff;
                Vec_push(&bin, &byte);
            }
            *data = Data_from_imm_bin(bin, self);
            return ParserMsg_new(parser.offset, NULL);
        }
    }

    Type_free(self);
    return ParserMsg_new(parser.offset, "unexpected token");
}

static ParserMsg Type_initialize_integer(in Type* self, inout Parser* parser, inout Vec* bin) {
    Parser parser_copy = *parser;

    u64 value = 0;
    PARSERMSG_UNWRAP(
        Parser_parse_number(&parser_copy, &value),
        (void)NULL
    );

    for(u32 i=0; i<self->size; i++) {
        u8 byte = (i < 8)?((value >> (i*8)) & 0xff):(0);
        Vec_push(bin, &byte);
    }

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

static ParserMsg Type_initialize_array(in Type* self, inout Parser* parser, inout Vec* bin) {
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "["),
        (void)NULL
    );

    for(u32 i=0; i<self->body.t_array.len; i++) {
        if(i != 0) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser_copy, ","),
                (void)NULL
            );
        }

        PARSERMSG_UNWRAP(
            Type_initialize(self->body.t_array.type, &parser_copy,  bin),
            (void)NULL
        );
    }

    Parser_parse_symbol(&parser_copy, ",");

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "]"),
        (void)NULL
    );

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

static ParserMsg Type_initialize_char(in Type* self, inout Parser* parser, inout Vec* bin) {
    if(strcmp(self->name, "char") != 0 || self->type != Type_Integer || self->size != 1) {
        return ParserMsg_new(parser->offset, "unexpected charactor");
    }
    Parser parser_copy = *parser;

    char code = 0;
    PARSERMSG_UNWRAP(
        Parser_parse_char(&parser_copy, &code),
        (void)NULL
    );

    u8 code_u8 = code;
    Vec_push(bin, &code_u8);

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

static ParserMsg Type_initialize_str(in Type* self, inout Parser* parser, inout Vec* bin) {
    if(strcmp(self->body.t_array.type->name, "char") != 0 || self->type != Type_Array) {
        return ParserMsg_new(parser->offset, "unexpected string");
    }

    Parser parser_copy = *parser;

    Vec string;
    PARSERMSG_UNWRAP(
        Parser_parse_string(&parser_copy, &string),
        (void)NULL
    );

    for(u32 i=0; i<self->body.t_array.len; i++) {
        if(i<Vec_len(&string)) {
            Vec_push(bin, Vec_index(&string, i));
        }else {
            u8 byte = 0;
            Vec_push(bin, &byte);
        }
    }

    Vec_free(string);
    *parser = parser_copy;

    return ParserMsg_new(parser->offset, NULL);
}

static ParserMsg Type_initialize_struct(in Type* self, inout Parser* parser, inout Vec* bin) {
    Parser parser_copy = *parser;

    if(self->name[0] != '\0') {
        PARSERMSG_UNWRAP(
            Parser_parse_keyword(&parser_copy, self->name), (void)NULL
        );
    }
    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "{"), (void)NULL
    );

    u32 bin_init_offset = Vec_len(bin);
    for(u32 i=0; i<Vec_len(&self->body.t_struct); i++) {
        if(i != 0) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser_copy, ","), (void)NULL
            );
        }

        StructMember* member = Vec_index(&self->body.t_struct, i);
        PARSERMSG_UNWRAP(
            Parser_parse_keyword(&parser_copy, member->name), (void)NULL
        );
        PARSERMSG_UNWRAP(Parser_parse_symbol(&parser_copy, ":"), (void)NULL);

        while(member->offset + bin_init_offset == Vec_len(bin)) {
            u8 byte = 0;
            Vec_push(bin, &byte);
        }

        PARSERMSG_UNWRAP(
            Type_initialize(&member->type, &parser_copy, bin), (void)NULL
        );
    }

    Parser_parse_symbol(&parser_copy, ",");

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "}"), (void)NULL
    );

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

static ParserMsg Type_initialize_enum(in Type* self, inout Parser* parser, inout Vec* bin) {
    for(u32 i=0; i<Vec_len(&self->body.t_enum); i++) {
        EnumMember* member = Vec_index(&self->body.t_enum, i);
        if(ParserMsg_is_success(Parser_parse_keyword(parser, member->name))) {
            for(u32 i=0; i<4; i++) {
                u8 byte = (member->value >> (i*8)) & 0xff;
                Vec_push(bin, &byte);
            }
            return ParserMsg_new(parser->offset, NULL);
        }
    }

    return ParserMsg_new(parser->offset, "unexpected enum");
}

ParserMsg Type_initialize(in Type* self, inout Parser* parser, inout Vec* bin) {
    switch(self->type) {
        case Type_Integer:
            if(Parser_start_with_symbol(parser, "\'")) {
                PARSERMSG_UNWRAP(
                    Type_initialize_char(self, parser, bin),
                    (void)NULL
                );
            }else {
                PARSERMSG_UNWRAP(
                    Type_initialize_integer(self, parser, bin),
                    (void)NULL
                );
            }
            break;
        case Type_Array:
            if(Parser_start_with_symbol(parser, "\"")) {
                PARSERMSG_UNWRAP(
                    Type_initialize_str(self, parser, bin),
                    (void)NULL
                );
            }else {
                PARSERMSG_UNWRAP(
                    Type_initialize_array(self, parser, bin),
                    (void)NULL
                );
            }
            break;
        case Type_Struct:
            PARSERMSG_UNWRAP(
                Type_initialize_struct(self, parser, bin),
                (void)NULL
            );
            break;
        case Type_Enum:
            PARSERMSG_UNWRAP(
                Type_initialize_enum(self, parser, bin),
                (void)NULL
            );
            break;
        default:
            return ParserMsg_new(parser->offset, "unexpected initializer");
    }

    return ParserMsg_new(parser->offset, NULL);
}

void Type_restrict_namespace(inout Type* self, in char* namespace) {
    snprintf(self->valid_path, 256, "%.255s", namespace);
}

Type Type_fn_from(in Vec* arguments) {
    assert(Vec_size(arguments) == sizeof(Variable));

    Type type;
    type.name[0] = '\0';
    type.valid_path[0] = '\0';
    type.type = Type_Fn;
    type.body.t_fn = Vec_new(sizeof(Data));
    for(u32 i=0; i<Vec_len(arguments); i++) {
        Variable* variable = Vec_index(arguments, i);
        Data data = Data_clone(&variable->data);
        Vec_push(&type.body.t_fn, &data);
    }
    type.size = 0;
    type.align = 0x10;

    return type;
}

SResult Type_dot_element(in Type* self, in char* element, out u32* offset, out Type* type) {
    assert(self != NULL && element != NULL && offset != NULL && type != NULL);

    if(self->type != Type_Struct) {
        return SResult_new("expected structure");
    }
    for(u32 i=0; i<Vec_len(&self->body.t_struct); i++) {
        StructMember* struct_member = Vec_index(&self->body.t_struct, i);
        if(strcmp(struct_member->name, element) == 0) {
            *offset = struct_member->offset;
            *type = Type_clone(&struct_member->type);

            return SResult_new(NULL);
        }
    }

    return SResult_new("unknown structure element");
}

SResult Type_refer_operator(in Type* src, in Generator* generator, out Type* type) {
    assert(src != NULL);
    assert(generator != NULL);
    assert(type != NULL);

    switch(src->type) {
        case Type_Ptr:
            *type = Type_clone(src->body.t_ptr);
            break;
        case Type_LazyPtr:
            SRESULT_UNWRAP(
                Generator_get_type(generator, src->body.t_lazy_ptr, type),
                (void)NULL
            );
            break;
        default:
            return SResult_new("expected pointer");
    }

    return SResult_new(NULL);
}

SResult Type_subscript(in Type* self, u32 index, out Type* type) {
    assert(self != NULL && type != NULL);

    switch(self->type) {
        case Type_Ptr:
            *type = Type_clone(self->body.t_ptr);
            break;
        case Type_Array:
            if(self->body.t_array.len <= index) {
                return SResult_new("out of range");
            }
            *type = Type_clone(self->body.t_array.type);
            break;
        default:
            return SResult_new("expected pointer or array");
    }

    return SResult_new(NULL);
}

Type Type_void(void) {
    Type type = {
        "void", "", Type_Integer, {}, 0, 1
    };

    return type;
}

Type Type_as_alias(Type self, in char* name) {
    assert(name != NULL);

    snprintf(self.name, 256, "%.255s", name);
    
    return self;
}

bool Type_cmp(in Type* self, in Type* other) {
    if(strcmp(self->name, other->name) != 0
        || strcmp(self->valid_path, other->valid_path) != 0
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
        case Type_LazyPtr:
            break;
        case Type_Fn:
            if(!Vec_cmp(&self->body.t_fn, &other->body.t_fn, Data_cmp_signature_for_vec)) {
                return false;
            }
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
        case Type_LazyPtr:
            break;
        case Type_Fn:
            type.body.t_fn = Vec_clone(&self->body.t_fn, Data_clone_for_vec);
            break;
    }
    return type;
}

void Type_print(in Type* self) {
    printf("Type { name: %s, valid_path: %s, type: %d, body: ", self->name, self->valid_path, self->type);
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
        case Type_LazyPtr:
            printf(".t_lazy_ptr: %s", self->body.t_lazy_ptr);
            break;
        case Type_Fn:
            printf(".fn: ");
            Vec_print(&self->body.t_fn, Data_free_for_vec);
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
        case Type_LazyPtr:
            break;
        case Type_Fn:
            Vec_free_all(self.body.t_fn, Data_free_for_vec);
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
    return ParserMsg_new(parser->offset, NULL);
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
    return strcmp(self->name, other->name) == 0
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

    return ParserMsg_new(parser->offset, NULL);
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


