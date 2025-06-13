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

ParserMsg Type_parse(inout Parser* parser, in Generator* generator, out Type* type) {
    Parser parser_copy = *parser;
    char token[256];
    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, token),
        (void)NULL
    );

    if(strcmp(token, "struct") == 0) {
        TODO();
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
            *type.body.t_ptr = Type_clone(self->body.t_ptr);
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
    }
}

void Type_free_for_vec(void* ptr) {
    Type* self = ptr;
    Type_free(*self);
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
        Storage_parse(&parser_copy, rbp_offset, &data->storage),
        (void)NULL
    );

    *parser = parser_copy;

    return SUCCESS_PARSER_MSG;
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

Generator Generator_new() {
    Generator generator = { Vec_from(TYPES, LEN(TYPES), sizeof(Type)) };
    return generator;
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
    snprintf(result.error, 255, "type \"%s\" is undefined", name);

    return result;
}

void Generator_print(in Generator* self) {
    printf("Generator { types: ");
    Vec_print(&self->types, Type_print_for_vec);
    printf(" }");
}

void Generator_free(Generator self) {
    Vec_free_all(self.types, Type_free_for_vec);
}

