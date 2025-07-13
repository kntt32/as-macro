#include <stdio.h>
#include <assert.h>
#include "gen.h"

ParserMsg Variable_parse(inout Parser* parser, in Generator* generator, inout i32* stack_offset, out Variable* variable) {
    // name: Data
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
    PARSERMSG_UNWRAP(
        Data_parse(&parser_copy, generator, stack_offset, &variable->data),
        (void)NULL
    );

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Variable_parse_global(inout Parser* parser, bool public_flag, inout Generator* generator) {
    Parser parser_copy = *parser;

    Variable variable;
    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, variable.name),
        (void)NULL
    );

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, ":"),
        (void)NULL
    );

    Type type;
    PARSERMSG_UNWRAP(
        Type_parse(&parser_copy, generator, &type),
        (void)NULL
    );

    variable.data = Data_from_mem(Rip, 0, variable.name, type);
    variable.valid_path[0] = '\0';
    if(public_flag) {
        snprintf(variable.valid_path, 256, "%.255s", Parser_path(parser));
    }

    if(ParserMsg_is_success(Parser_parse_symbol(&parser_copy, "="))) {
        PARSERMSG_UNWRAP(
            ParserMsg_from_sresult(Generator_append_label(generator, ".data", variable.name, public_flag, Label_Object), parser->offset),
            Variable_free(variable)
        );

        Vec bin = Vec_new(sizeof(u8));
        PARSERMSG_UNWRAP(
            Type_initialize(&variable.data.type, &parser_copy, &bin),
            Vec_free(bin);Variable_free(variable)
        );

        assert(Vec_len(&bin) == variable.data.type.size);
        for(u32 i=0; i<Vec_len(&bin); i++) {
            u8* byte = Vec_index(&bin, i);
            PARSERMSG_UNWRAP(
                ParserMsg_from_sresult(
                    Generator_append_binary(generator, ".data", *byte), parser->offset
                ),
                Vec_free(bin);Variable_free(variable)
            );
        }

        Vec_free(bin);
    }else {
        PARSERMSG_UNWRAP(
            ParserMsg_from_sresult(Generator_append_label(generator, ".bss", variable.name, public_flag, Label_Object), parser->offset),
            Variable_free(variable)
        );
        for(u32 i=0; i<variable.data.type.size; i++) {
            PARSERMSG_UNWRAP(
                ParserMsg_from_sresult(
                    Generator_append_binary(generator, ".bss", 0), parser->offset
                ),
                Variable_free(variable)
            );
        }
    }

    PARSERMSG_UNWRAP(
        ParserMsg_from_sresult(Generator_end_label(generator, variable.name), parser->offset),
        Variable_free(variable)
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

void Variable_restrict_namespace(inout Variable* self, in char* namespace) {
    assert(self != NULL && namespace != NULL);

    snprintf(self->valid_path, 256, "%.255s", namespace);
}

bool Variable_cmp(in Variable* self, in Variable* other) {
    assert(self != NULL && other != NULL);
    
    if(strcmp(self->name, other->name) != 0 || strcmp(self->valid_path, other->valid_path) != 0 || !Data_cmp(&self->data, &other->data)) {
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
    printf("Variable { name: %s, valid_path: %s, data: ", self->name, self->valid_path);
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

