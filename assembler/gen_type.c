#include <stdio.h>
#include <assert.h>
#include "types.h"
#include "gen.h"
#include "util.h"

Vec Type_primitives(void) {
    Type primitives[] = {
        {"void", "", Type_Integer, {}, 0, 1},
        {"bool", "", Type_Integer, {}, 1, 1},
        {"char", "", Type_Integer, {}, 1, 1},
        {"i8",  "", Type_Integer, {}, 1, 1},
        {"u8", "", Type_Integer, {}, 1, 1},
        {"i16", "", Type_Integer, {}, 2, 2},
        {"u16", "", Type_Integer, {}, 2, 2},
        {"i32", "", Type_Integer, {}, 4, 4},
        {"u32", "", Type_Integer, {}, 4, 4},
        {"i64", "", Type_Integer, {}, 8, 8},
        {"u64", "", Type_Integer, {}, 8, 8},
        {"f32", "", Type_Floating, {}, 4, 4},
        {"f64", "", Type_Floating, {}, 8, 8},
        {"b8", "", Type_Integer, {}, 1, 1},
        {"b16", "", Type_Integer, {}, 2, 2},
        {"b32", "", Type_Integer, {}, 4, 4},
        {"b64", "", Type_Integer, {}, 8, 8},
        {"bin", "", Type_Integer, {}, 1, 1},
    };
    Vec types = Vec_from(primitives, LEN(primitives), sizeof(Type));

    return types;
}

static ParserMsg Type_parse_ptr_lazyptr(inout Parser* parser, out Type* type) {
    char token[256];
    PARSERMSG_UNWRAP(
        Parser_parse_ident(parser, token), (void)NULL
    );

    wrapped_strcpy(type->name, "*", sizeof(type->name));
    wrapped_strcat(type->name, token, sizeof(type->name));
    type->valid_path[0] = '\0';
    type->type = Type_LazyPtr;
    wrapped_strcpy(type->body.t_lazy_ptr.name, token, sizeof(type->body.t_lazy_ptr.name));
    wrapped_strcpy(type->body.t_lazy_ptr.path, Parser_path(parser), sizeof(type->body.t_lazy_ptr.path));
    type->size = 8;
    type->align = 8;

    return ParserMsg_new(parser->offset, NULL);
}

static ParserMsg Type_parse_ptr(inout Parser* parser, in Generator* generator, out Type* type) {
    assert(parser != NULL && generator != NULL && type != NULL);

    Type child_type;
    if(ParserMsg_is_success(Type_parse(parser, generator, &child_type))) {
        wrapped_strcpy(type->name, "*", sizeof(type->name));
        wrapped_strcat(type->name, child_type.name, sizeof(type->name));
        
        wrapped_strcpy(type->valid_path, child_type.valid_path, sizeof(type->valid_path));

        type->type = Type_Ptr;
        type->body.t_ptr = malloc(sizeof(Type));
        UNWRAP_NULL(type->body.t_ptr);
        *type->body.t_ptr = child_type;
        type->size = 8;
        type->align = 8;       
    }else {
        PARSERMSG_UNWRAP(
            Type_parse_ptr_lazyptr(parser, type), (void)NULL
        );
    }

    return ParserMsg_new(parser->offset, NULL);
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

static ParserMsg Type_parse_union_member(Parser parser, in Generator* generator, out Type* type) {
    type->body.t_union = Vec_new(sizeof(StructMember));
    type->align = 0x1;
    type->size = 0;
    
    while(!Parser_is_empty(&parser)) {
        u64 align = 0x1;
        u32 size = 0;
        StructMember member;
        PARSERMSG_UNWRAP(
            StructMember_parse(&parser, generator, &align , &size, &member),
            Vec_free(type->body.t_union)
        );

        Vec_push(&type->body.t_union, &member);

        if(type->align < align) {
            type->align = align;
        }
        if(type->size < size) {
            type->size = size;
        }
    }

    return ParserMsg_new(parser.offset, NULL);
}

ParserMsg Type_parse_union(inout Parser* parser, in Generator* generator, out Type* type) {
    Parser parser_copy = *parser;

    type->type = Type_Union;
    if(!ParserMsg_is_success(Parser_parse_ident(&parser_copy, type->name))) {
        type->name[0] = '\0';
    }
    type->valid_path[0] = '\0';

    Parser block_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_block(&parser_copy, &block_parser),
        (void)NULL
    );

    PARSERMSG_UNWRAP(
        Type_parse_union_member(block_parser, generator, type), (void)NULL
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

static ParserMsg Type_parse_fn(inout Parser* parser, in Generator* generator, out Type* type) {
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
            Data_parse(&paren_parser, generator, &dummy, NULL, &argument), Vec_free_all(arguments, Data_free_for_vec)
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

ParserMsg Type_parse_fnptr(inout Parser* parser, in Generator* generator, out Type* type) {
    Type fn_type;
    PARSERMSG_UNWRAP(
        Type_parse_fn(parser, generator, &fn_type), (void)NULL
    );

    type->name[0] = '\0';
    type->valid_path[0] = '\0';
    type->type = Type_Ptr;
    type->body.t_ptr = malloc(sizeof(Type));
    UNWRAP_NULL(type->body.t_ptr);
    *type->body.t_ptr = fn_type;
    type->size = 8;
    type->align = 8;

    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Type_parse(inout Parser* parser, in Generator* generator, out Type* type) {
    Parser parser_copy = *parser;

    char token[256];
    if(!ParserMsg_is_success(Parser_parse_ident(&parser_copy, token))) {
        if(ParserMsg_is_success(Parser_parse_symbol(&parser_copy, "*"))) {
            PARSERMSG_UNWRAP(
                Type_parse_ptr(&parser_copy, generator, type),
                (void)NULL
            );
        }else {
            if(!ParserMsg_is_success(Type_parse_array(&parser_copy, generator, type))) {
                return ParserMsg_new(parser->offset, "expected type");
            }
        }
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
    }else if(strcmp(token, "union") == 0) {
        PARSERMSG_UNWRAP(
            Type_parse_union(&parser_copy, generator, type),
            (void)NULL
        );
    }else if(strcmp(token, "fn") == 0) {
        PARSERMSG_UNWRAP(
            Type_parse_fnptr(&parser_copy, generator, type),
            (void)NULL
        );
    }else if(!SResult_is_success(
        Generator_get_type(generator, token, Parser_path(parser), type)
    )) {
        return ParserMsg_new(parser->offset, "expected type");
    }

    if(type->valid_path[0] != '\0' && strcmp(Parser_path(&parser_copy), type->valid_path) != 0) {
        return ParserMsg_new(parser->offset, "out of namespace");
    }

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

    if(Parser_skip_symbol(&parser_copy, "]")) {
        u8 byte = 0;
        for(u32 i=0; i<self->size; i++) {
            Vec_push(bin, &byte);
        }
    }else {
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
    }

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
    Parser parser_copy = *parser;
    PARSERMSG_UNWRAP(
        Parser_parse_keyword(&parser_copy, self->name), (void)NULL
    );
    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "."), (void)NULL
    );

    for(u32 i=0; i<Vec_len(&self->body.t_enum); i++) {
        EnumMember* member = Vec_index(&self->body.t_enum, i);
        if(ParserMsg_is_success(Parser_parse_keyword(&parser_copy, member->name))) {
            for(u32 i=0; i<4; i++) {
                u8 byte = (member->value >> (i*8)) & 0xff;
                Vec_push(bin, &byte);
            }
            *parser = parser_copy;
            return ParserMsg_new(parser->offset, NULL);
        }
    }

    return ParserMsg_new(parser->offset, "unexpected enum");
}

static ParserMsg Type_initialize_bool(in Type* self, inout Parser* parser, inout Vec* bin) {
    if(Parser_skip_keyword(parser, "true")) {
        u8 true_byte = 0x1;
        Vec_push(bin, &true_byte);
    }else if(Parser_skip_keyword(parser, "false")) {
        u8 false_byte = 0x0;
        Vec_push(bin, &false_byte);
    }else {
        return ParserMsg_new(parser->offset, "expected true or false");
    }

    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Type_initialize(in Type* self, inout Parser* parser, inout Vec* bin) {
    switch(self->type) {
        case Type_Integer:
            if(strcmp(self->name, "bool") == 0) {
                PARSERMSG_UNWRAP(
                    Type_initialize_bool(self, parser, bin),
                    (void)NULL
                )
            }else if(Parser_start_with_symbol(parser, "\'")) {
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
    wrapped_strcpy(self->valid_path, namespace, sizeof(self->valid_path));
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

    Vec* members;
    switch(self->type) {
        case Type_Struct:
            members = &self->body.t_struct;
            break;
        case Type_Union:
            members = &self->body.t_union;
            break;
        case Type_Alias:
            return Type_dot_element(self->body.t_alias, element, offset, type);
        default:
            return SResult_new("expected struct or union");
    }

    for(u32 i=0; i<Vec_len(&self->body.t_struct); i++) {
        StructMember* member = Vec_index(members, i);
        if(strcmp(member->name, element) == 0) {
            *offset = member->offset;
            *type = Type_clone(&member->type);

            return SResult_new(NULL);
        }
    }

    return SResult_new("unknown structure element");
}

SResult Type_ref(in Type* src, in Generator* generator, out Type* type) {
    assert(src != NULL);
    assert(generator != NULL);
    assert(type != NULL);

    switch(src->type) {
        case Type_Ptr:
            *type = Type_clone(src->body.t_ptr);
            break;
        case Type_LazyPtr:
            SRESULT_UNWRAP(
                Generator_get_type(generator, src->body.t_lazy_ptr.name, src->body.t_lazy_ptr.path, type),
                (void)NULL
            );
            break;
        case Type_Alias:
            SRESULT_UNWRAP(
                Type_ref(src->body.t_alias, generator, type), (void)NULL
            );
            break;
        default:
            return SResult_new("expected pointer");
    }

    return SResult_new(NULL);
}

SResult Type_subscript(in Type* self, in Generator* generator, u32 index, out Type* type) {
    assert(self != NULL && type != NULL);
    assert(self->name[0] != '\0');

    switch(self->type) {
        case Type_Array:
            if(self->body.t_array.len <= index) {
                return SResult_new("out of range");
            }
            *type = Type_clone(self->body.t_array.type);
            break;
        case Type_Alias:
            SRESULT_UNWRAP(
                Type_subscript(self->body.t_alias, generator, index, type), (void)NULL
            );
            break;
        default:
            return SResult_new("expected array");
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

    Type type;
    wrapped_strcpy(type.name, name, sizeof(type.name));
    type.type = Type_Alias;
    type.body.t_alias = malloc(sizeof(Type));
    UNWRAP_NULL(type.body.t_alias);
    *type.body.t_alias = self;
    type.size = self.size;
    type.align = self.align;
    
    return type;
}

bool Type_cmp(in Type* self, in Type* other) {
    if(self->name[0] == '\0' || other->name[0] == '\0'
        || strcmp(self->name, other->name) != 0
        || strcmp(self->valid_path, other->valid_path) != 0
        || self->type != other->type
        || self->size != other->size
        || self->align != other->align) {
        return false;
    }

    return true;
}

static bool Type_match_bound(in Type* self, in Type* other) {
    if((strcmp(self->name, "*void") == 0 && (other->type == Type_Ptr || other->type == Type_LazyPtr))
        || (strcmp(other->name, "*void") == 0 && (self->type == Type_Ptr || self->type == Type_LazyPtr))) {
        return true;
    }
    if(((other->type == Type_Ptr || other->type == Type_Integer
        || other->type == Type_LazyPtr || other->type == Type_Enum
        || other->type == Type_Floating)
        && ((strcmp(self->name, "b8") == 0 && other->size == 1)
        || (strcmp(self->name, "b16") == 0 && other->size == 2)
        || (strcmp(self->name, "b32") == 0 && other->size == 4)
        || (strcmp(self->name, "b64") == 0 && other->size == 8)))
        || (strcmp(self->name, "bin") == 0)) {
        return true;
    }

    return false;
}

static bool Type_match_alias(in Type* self, in Type* other) {
    if(self->type == Type_Alias) {
        Type* self_type = self->body.t_alias;
        return Type_match(self_type, other);
    }
    if(other->type == Type_Alias) {
        Type* other_type = other->body.t_alias;
        return Type_match(self, other_type);
    }
    return false;
}

bool Type_match(in Type* self, in Type* other) {
    assert(self != NULL && other != NULL);

    if(Type_match_bound(self, other)) {
        return true;
    }

    if(Type_match_alias(self, other)) {
        return true;
    }

    if(self->type != other->type) {
        return false;
    }

    if(self->name[0] == '\0' || other->name[0] == '\0'
        || strcmp(self->valid_path, other->valid_path) != 0
        || strcmp(self->name, other->name) != 0
        || self->size != other->size
        || self->align != other->align) {
        return false;
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
        case Type_Union:
            type.body.t_union = Vec_clone(&self->body.t_union, StructMember_clone_for_vec);
            break;
        case Type_Floating:
            break;
        case Type_LazyPtr:
            break;
        case Type_Alias:
            type.body.t_alias = malloc(sizeof(Type));
            UNWRAP_NULL(type.body.t_alias);
            *type.body.t_alias = Type_clone(self->body.t_alias);
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
        case Type_Union:
            printf(".t_union: ");
            Vec_print(&self->body.t_union, StructMember_print_for_vec);
            break;
        case Type_Floating:
            printf("none");
            break;
        case Type_LazyPtr:
            printf(".t_lazy_ptr: { name: %s, path: %s }", self->body.t_lazy_ptr.name, self->body.t_lazy_ptr.path);
            break;
        case Type_Alias:
            printf(".t_alias: ");
            Type_print(self->body.t_alias);
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
        case Type_Union:
            Vec_free_all(self.body.t_union, StructMember_free_for_vec);
            break;
        case Type_Floating:
            break;
        case Type_LazyPtr:
            break;
        case Type_Alias:
            Type_free(*self.body.t_alias);
            free(self.body.t_alias);
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


