#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "types.h"
#include "gen.h"
#include "util.h"

ParserMsg PartialType_parse(Parser parser, out PartialType* partial_type) {
    partial_type->pub_flag = Parser_skip_keyword(&parser, "pub");

    if(Parser_start_with(&parser, "struct")) {
        partial_type->type = PartialType_Struct;
    }else if(Parser_start_with(&parser, "enum")) {
        partial_type->type = PartialType_Enum;
    }else if(Parser_start_with(&parser, "union")) {
        partial_type->type = PartialType_Union;
    }else {
        partial_type->type = PartialType_Other;
    }

    if(partial_type->type != PartialType_Other) {
        PARSERMSG_UNWRAP(
            Parser_parse_ident(&parser, partial_type->name.name),
            (void)NULL
        );
        PARSERMSG_UNWRAP(
            Parser_parse_block(&parser, &partial_type->parser),
            (void)NULL
        );
    }else {
        partial_type->parser = parser;
    }

    return ParserMsg_new(parser.offset, NULL);
}

static ParserMsg PartialType_as_type_struct(in* PartialType* self, in Vec* partial_types, inout Vec* expand_stack, out Type* type) {
    assert(self->type == PartialType_Struct);

    Parser parser = self->parser;

    wrapped_strcpy(type->name, self->name.name, sizeof(type->name));
    if(self->pub_flag) {
        type->valid_path[0] = '\0';
    }else {
        wrapped_strcpy(type->valid_path, Parser_path(&parser), sizeof(type->valid_path));
    }
    type->type = Type_Struct;
    type->body.t_struct = Vec_new(sizeof(StructMember));
    type->align = 1;
    type->size = 0;
    while(!Parser_is_empty(&parser)) {
        StructMember member;
        PARSERMSG_UNWRAP(
            StructMember_parse_from_partial_type(&parser, partial_types, expand_stack, &type->align, &type->size, &member),
            Vec_free_all(type->body.t_struct, StructMember_free_for_vec)
        );
        Vec_push(&type->body.t_struct, &member);

        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, ","),
                Vec_free_all(type->body.t_struct, StructMember_free_for_vec)
            );
        }
    }

    return ParserMsg_new(NULL);
}

static ParserMsg PartialType_as_type_enum(in* PartialType* self, in Vec* partial_types, inout Vec* expand_stack, out Type* type) {
    assert(self->type == PartialType_enum);

    Parser parser = self->parser;

    wrapped_strcpy(type->name, self->name.name, sizeof(type->name));
    if(self->pub_flag) {
        type->valid_path[0] = '\0';
    }else {
        wrapped_strcpy(type->valid_path, Parser_path(&parser), sizeof(type->valid_path));
    }
    type->type = Type_Enum;
    type->body.t_enum = Vec_new(sizeof(EnumMember));
    type->align = 4;
    type->size = 4;
    i32 enum_value = 0;
    while(!Parser_is_empty(&parser)) {
        EnumMember member;
        PARSERMSG_UNWRAP(
            EnumMember_parse(&parser, &enum_value, &member),
            Vec_free(type->body.t_enum)
        );
        Vec_push(&type->body.t_enum, &member);

        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, ","),
                Vec_free(type->body.t_struct)
            );
        }
    }

    return ParserMsg_new(NULL);
}

static ParserMsg PartialType_as_type_union(in* PartialType* self, in Vec* partial_types, inout Vec* expand_stack, out Type* type) {
    assert(self->type == PartialType_Union);

    Parser parser = self->parser;

    wrapped_strcpy(type->name, self->name.name, sizeof(type->name));
    if(self->pub_flag) {
        type->valid_path[0] = '\0';
    }else {
        wrapped_strcpy(type->valid_path, Parser_path(&parser), sizeof(type->valid_path));
    }
    type->type = Type_Union;
    type->body.t_union = Vec_new(sizeof(UnionMember));
    type->align = 1;
    type->size = 0;
    while(!Parser_is_empty(&parser)) {
        u64 align = 0x1;
        u32 size = 0;

        StructMember member;
        PARSERMSG_UNWRAP(
            StructMember_parse_from_partial_type(&parser, partial_types, expand_stack, &align, &size, &member),
            Vec_free_all(type->body.t_union, StructMember_free_for_vec)
        );
        Vec_push(&type->body.t_union, &member);

        if(type->align < align) {
            type->align = align;
        }
        if(type->size < size) {
            type->size = size;
        }

        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, ","),
                Vec_free_all(type->body.t_union, StructMember_free_for_vec)
            );
        }
    }

    return ParserMsg_new(NULL);
}

static ParserMsg PartialType_as_type_other(in PartialType* self, in Vec* partial_types, inout Vec* expand_stack, out Type* type) {

}

static ParserMsg PartialType_as_type_(in PartialType* self, in Vec* partial_types, inout Vec* expand_stack, out Type* type) {
    assert(Vec_size(partial_type) == sizeof(PartialType));

    Vec_push(expand_stack, self);

    switch(self->type) {
        case PartialType_Primitive:
            
        case PartialType_Struct:
            PARSERMSG_UNWRAP(
                PartialType_as_type_struct(self, partial_types, expand_stack, type), (void)NULL
            );
            break;
        case PartialType_Enum:
            PARSERMSG_UNWRAP(
                PartialType_as_type_enum(self, partial_types, expand_stack, type), (void)NULL
            );
            break;
        case PartialType_Union:
            PARSERMSG_UNWRAP(
                PartialType_as_type_union(self, partial_types, expand_stack, type), (void)NULL
            );
            break;
        case PartialType_Other:
            PARSERMSG_UNWRAP(
                PartialType_as_type_other(self, partial_types, expand_stack, type), (void)NULL
            );
            break;
    }

    Vec_pop(expand_stack, NULL);

    return ParserMsg_new(parser_copy.offset, NULL);
}

ParserMsg PartialType_as_type(in PartialType* self, in Vec* partial_types, out Type* type) {
    Vec expand_stack = Vec_new(sizeof(PartialType));

    ParserMsg parser_msg = PartialType_as_type_(self, partial_types, &expand_stack, type);
    Vec_free(expand_stack);

    return parser_msg;
}

