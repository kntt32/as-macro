#include <stdio.h>
#include <assert.h>
#include "gen.h"

Variable Variable_new(in char* name, in char* valid_path, Data data, bool defer_flag) {
    Variable variable;

    snprintf(variable.name, 256, "%.255s", name);
    snprintf(variable.valid_path, 256, "%.255s", valid_path);
    variable.data = data;
    variable.defer_flag = defer_flag;

    return variable;
}

ParserMsg Variable_parse(inout Parser* parser, in Generator* generator, inout i32* stack_offset, out Variable* variable) {
    // (defer) name: Data
    Parser parser_copy = *parser;

    variable->valid_path[0] = '\0';

    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, variable->name),
        (void)NULL
    );
    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, ":"),
        (void)NULL
    );
    variable->defer_flag = ParserMsg_is_success(Parser_parse_keyword(&parser_copy, "defer"));
    PARSERMSG_UNWRAP(
        Data_parse(&parser_copy, generator, stack_offset, &variable->data),
        (void)NULL
    );

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

static ParserMsg Variable_parse_static_parse(inout Parser* parser, bool public_flag, in Generator* generator, out Variable* variable) {
    PARSERMSG_UNWRAP(
        Parser_parse_ident(parser, variable->name),
        (void)NULL
    );

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(parser, ":"),
        (void)NULL
    );

    Type type;
    PARSERMSG_UNWRAP(
        Type_parse(parser, generator, &type),
        (void)NULL
    );

    variable->data = Data_from_mem(Rip, 0, variable->name, type);
    variable->defer_flag = false;
    variable->valid_path[0] = '\0';
    if(public_flag) {
        snprintf(variable->valid_path, 256, "%.255s", Parser_path(parser));
    }

    return ParserMsg_new(parser->offset, NULL);
}

static ParserMsg Variable_parse_static_init(inout Parser* parser, bool static_flag, in Variable* variable, in char* label, inout Generator* generator) {
    if(ParserMsg_is_success(Parser_parse_symbol(parser, "="))) {
        PARSERMSG_UNWRAP(
            ParserMsg_from_sresult(Generator_append_label(generator, ".data", label, !static_flag, Label_Object), parser->offset),
            (void)NULL
        );

        Vec bin = Vec_new(sizeof(u8));
        PARSERMSG_UNWRAP(
            Type_initialize(&variable->data.type, parser, &bin),
            Vec_free(bin);
        );

        assert(Vec_len(&bin) == variable->data.type.size);
        for(u32 i=0; i<Vec_len(&bin); i++) {
            u8* byte = Vec_index(&bin, i);
            PARSERMSG_UNWRAP(
                ParserMsg_from_sresult(
                    Generator_append_binary(generator, ".data", *byte), parser->offset
                ),
                Vec_free(bin);
            );
        }

        Vec_free(bin);
    }else {
        PARSERMSG_UNWRAP(
            ParserMsg_from_sresult(Generator_append_label(generator, ".bss", label, !static_flag, Label_Object), parser->offset),
            (void)NULL
        );
        for(u32 i=0; i<variable->data.type.size; i++) {
            PARSERMSG_UNWRAP(
                ParserMsg_from_sresult(
                    Generator_append_binary(generator, ".bss", 0), parser->offset
                ),
                (void)NULL
            );
        }
    }

    PARSERMSG_UNWRAP(
        ParserMsg_from_sresult(Generator_end_label(generator, label), parser->offset), (void)NULL
    );

    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Variable_parse_static(inout Parser* parser, bool public_flag, bool static_flag, inout Generator* generator) {
    Parser parser_copy = *parser;

    Variable variable;
    PARSERMSG_UNWRAP(
        Variable_parse_static_parse(&parser_copy, public_flag, generator, &variable), (void)NULL
    );

    PARSERMSG_UNWRAP(
        Variable_parse_static_init(&parser_copy, static_flag, &variable, variable.name, generator),
        Variable_free(variable);
    );

    PARSERMSG_UNWRAP(
        ParserMsg_from_sresult(
            Generator_add_global_variable(generator, variable), parser->offset
        ),
        (void)NULL
    );

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Variable_parse_static_local(inout Parser* parser, in Generator* generator, out Variable* variable) {
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Variable_parse_static_parse(&parser_copy, false, generator, variable), (void)NULL
    );

    char label[256];
    snprintf(label, 256, ".%.254s", variable->name);
    assert(
        SResult_is_success(Storage_replace_label(&variable->data.storage, label))
    );
    PARSERMSG_UNWRAP(
        Variable_parse_static_init(&parser_copy, false, variable, label, generator),
        Variable_free(*variable)
    );

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

static ParserMsg Variable_parse_const_parse_(inout Parser* parser, bool public_flag, in Generator* generator, out Variable* variable) {
    Type type;
    
    PARSERMSG_UNWRAP(
        Parser_parse_ident(parser, variable->name), (void)NULL
    );

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(parser, ":"), (void)NULL
    );

    PARSERMSG_UNWRAP(
        Type_parse(parser, generator, &type), (void)NULL
    );

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(parser, "="),
        Type_free(type)
    );

    if(public_flag) {
        variable->valid_path[0] = '\0';
    }else {
        snprintf(variable->valid_path, 256, "%.255s", Parser_path(parser));
    }

    Vec bin = Vec_new(sizeof(u8));
    PARSERMSG_UNWRAP(
        Type_initialize(&type, parser, &bin),
        Vec_free(bin);Type_free(type);
    );
    variable->data = Data_from_imm_bin(bin, type);
    variable->defer_flag = false;

    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Variable_parse_const(inout Parser* parser, bool public_flag, inout Generator* generator) {
    Parser parser_copy = *parser;

    Variable variable;
    PARSERMSG_UNWRAP(
        Variable_parse_const_parse_(&parser_copy, public_flag, generator, &variable), (void)NULL
    );

    PARSERMSG_UNWRAP(
        ParserMsg_from_sresult(
            Generator_add_global_variable(generator, variable), parser->offset
        ),
        (void)NULL
    );

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Variable_parse_const_local(inout Parser* parser, inout Generator* generator, out Variable* variable) {
    Parser parser_copy = *parser;
    PARSERMSG_UNWRAP(
        Variable_parse_const_parse_(&parser_copy, false, generator, variable),
        (void)NULL
    );

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

Variable Variable_from_function(in char* name, in char* valid_path, in Vec* arguments) {
    Variable variable;
    snprintf(variable.name, 256, "%.255s", name);
    Type type = Type_fn_from(arguments);
    variable.data = Data_from_mem(Rip, 0, name, type);
    snprintf(variable.valid_path, 256, "%.255s", valid_path);
    variable.defer_flag = false;

    return variable;
}

void Variable_restrict_namespace(inout Variable* self, in char* namespace) {
    assert(self != NULL && namespace != NULL);

    snprintf(self->valid_path, 256, "%.255s", namespace);
}

bool Variable_cmp(in Variable* self, in Variable* other) {
    assert(self != NULL && other != NULL);
    
    if(self->defer_flag != other->defer_flag
        || strcmp(self->name, other->name) != 0
        || strcmp(self->valid_path, other->valid_path) != 0
        || !Data_cmp(&self->data, &other->data)) {
        return false;
    }
    
    return true;
}

bool Variable_cmp_for_vec(in void* left, in void* right) {
    return Variable_cmp(left, right);
}

Variable Variable_clone(in Variable* self) {
    Variable variable;
    
    strcpy(variable.name, self->name);
    strcpy(variable.valid_path, self->valid_path);
    variable.data = Data_clone(&self->data);
    variable.defer_flag = self->defer_flag;

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

Vec Variables_from_datas(in Vec* datas) {
    assert(Vec_size(datas) == sizeof(Data));

    Vec vec = Vec_new(sizeof(Variable));
    for(u32 i=0; i<Vec_len(datas); i++) {
        Data data = Data_clone(Vec_index(datas, i));
        Variable variable = {
            "", "",
            data, false
        };

        Vec_push(&vec, &variable);
    }

    return vec;
}

void Variable_print(in Variable* self) {
    printf("Variable { name: %s, valid_path: %s, data: ", self->name, self->valid_path);
    Data_print(&self->data);
    printf(" defer_flag: %s }", BOOL_TO_STR(self->defer_flag));
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

