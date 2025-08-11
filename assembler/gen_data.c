#include <stdio.h>
#include <assert.h>
#include "gen.h"
#include "util.h"

ParserMsg Data_parse(inout Parser* parser, in Generator* generator, inout i32* stack_offset, optional in Register* auto_reg, out Data* data) {
    assert(parser != NULL && generator != NULL && stack_offset != NULL && data != NULL);
    // Data @ Storage
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Type_parse(&parser_copy, generator, &data->type),
        (void)NULL
    );
    if(strcmp(data->type.name, "bin") == 0) {
        return ParserMsg_new(parser->offset, "construct Data with Type \"bin\" is disallowed");
    }
    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "@"),
        Type_free(data->type)
    );
    PARSERMSG_UNWRAP(
        Storage_parse(&parser_copy, stack_offset, auto_reg, &data->type, &data->storage),
        Type_free(data->type)
    );

    *parser = parser_copy;

    return ParserMsg_new(parser->offset, NULL);
}

bool Data_cmp(in Data* self, in Data* other) {
    assert(self != NULL && other != NULL);
    
    return Type_cmp(&self->type, &other->type) && Storage_cmp(&self->storage, &other->storage);
}

bool Data_cmp_for_vec(in void* left, in void* right) {
    return Type_cmp(left, right);
}

bool Data_cmp_signature(in Data* self, in Data* other) {
    if(self->storage.type == StorageType_mem && other->storage.type == StorageType_mem) {
        return Type_cmp(&self->type, &other->type);
    }

    return Data_cmp(self, other);
}

bool Data_cmp_signature_for_vec(in void* left, in void* right) {
    return Data_cmp_signature(left, right);
}

Data Data_from_register(Register reg) {
    Data data = {
        {"i64", "", Type_Integer, {}, 8, 8},
        {StorageType_reg, {.reg = reg}}
    };
    return data;
}

Data Data_from_imm(u64 imm) {
    Type type;
    u32 len = 0;
    if(imm <= 0xff) {
        Type tmp = {"i8", "", Type_Integer, {}, 1, 1};
        type = tmp;
        len = 1;
    }else if(imm <= 0xffff) {
        Type tmp = {"i16", "", Type_Integer, {}, 2, 2};
        type = tmp;
        len = 2;
    }else if(imm <= 0xffffffff) {
        Type tmp = {"i32", "", Type_Integer, {}, 4, 4};
        type = tmp;
        len = 4;
    }else {
        Type tmp = {"i64", "", Type_Integer, {}, 8, 8};
        type = tmp;
        len = 8;
    }
    Vec value = Vec_new(sizeof(u8));
    for(u32 i=0; i<len; i++) {
        u8 byte = (imm >> (i*8)) & 0xff;
        Vec_push(&value, &byte);
    }

    Data data = {
        type,
        {StorageType_imm, {.imm = {Imm_Value, {.value = value}}}}
    };
    return data;
}

Data Data_from_imm_bin(Vec bin, Type type) {
    assert(type.size == Vec_len(&bin));

    Data data = {
        type,
        {StorageType_imm, {.imm = {Imm_Value, {.value = bin}}}}
    };

    return data;
}

Data Data_from_label(in char* label) {
    Data data = {
        {"i32", "", Type_Integer, {}, 4, 4},
        {StorageType_imm, {.imm = {Imm_Label, {.label = ""}}}}
    };
    strncpy(data.storage.body.imm.body.label, label, 255);

    return data;
}

Data Data_from_mem(Register reg, i32 offset, in optional char* label, Type type) {
    Data data = {
        type,
        {StorageType_mem, {.mem = {reg, {offset, ""}}}}
    };
    if(label != NULL) {
        wrapped_strcpy(data.storage.body.mem.disp.label, label, 256);
    }

    return data;
}

Data Data_true(void) {
    Vec true_vec = Vec_new(sizeof(u8));
    u8 true_u8 = 1;
    Vec_push(&true_vec, &true_u8);
    Imm imm = {
        Imm_Value,
        {.value = true_vec}
    };
    Data data = {
        {"bool", "", Type_Integer, {}, 1, 1},
        {StorageType_imm, {.imm = imm}}
    };

    return data;
}

Data Data_false(void) {
    Vec false_vec = Vec_new(sizeof(u8));
    u8 false_u8 = 0;
    Vec_push(&false_vec, &false_u8);
    Imm imm = {
        Imm_Value,
        {.value = false_vec}
    };
    Data data = {
        {"bool", "", Type_Integer, {}, 1, 1},
        {StorageType_imm, {.imm = imm}}
    };

    return data;
}

Data Data_null(void) {
    Vec null_vec = Vec_new(sizeof(u8));
    for(u32 i=0; i<8; i++) {
        u8 zero = 0;
        Vec_push(&null_vec, &zero);
    }
    Imm imm = {
        Imm_Value,
        {.value = null_vec}
    };

    Type* void_type_ptr = malloc(sizeof(Type));
    UNWRAP_NULL(void_type_ptr);
    *void_type_ptr = Type_void();
    Data data = {
        {"void*", "", Type_Ptr, {.t_ptr = void_type_ptr}, 8, 8},
        {StorageType_imm, {.imm = imm}}
    };

    return data;
}

Data Data_from_char(u8 code) {
    Vec data_vec = Vec_new(sizeof(u8));
    Vec_push(&data_vec, &code);
    Data data = {
        {"char", "", Type_Integer, {}, 1, 1},
        {StorageType_imm, {.imm = {Imm_Value, {.value = data_vec}}}}
    };

    return data;
}

Data Data_clone(in Data* self) {
    Data data;
    
    data.type = Type_clone(&self->type);
    data.storage = Storage_clone(&self->storage);
    
    return data;
}

void Data_clone_for_vec(out void* dst, in void* src) {
    Data* data = dst;
    *data = Data_clone(src);
}

SResult Data_dot_operator(in Data* left, in char* element, out Data* data) {
    assert(left != NULL && data != NULL);

    u32 offset = 0;
    SRESULT_UNWRAP(
        Type_dot_element(&left->type, element, &offset, &data->type),
        (void)NULL
    );

    data->storage = Storage_clone(&left->storage);
    SRESULT_UNWRAP(
        Storage_add_offset(&data->storage, offset),
        Storage_free(data->storage);Type_free(data->type);
    );

    return SResult_new(NULL);
}

SResult Data_as_integer(in Data* self, out u64* integer) {
    if(self->type.type != Type_Integer && self->type.size <= 8) {
        return SResult_new("expected integer");
    }
    if(self->storage.type != StorageType_imm) {
        return SResult_new("expected imm");
    }

    Imm* imm = &self->storage.body.imm;
    if(imm->type != Imm_Value) {
        return SResult_new("expected imm value");
    }
    
    *integer = 0;
    Vec* value = &imm->body.value;
    assert(Vec_len(value) <= 8);
    for(u32 i=0; i<Vec_len(value); i++) {
        u8* byte_ptr = Vec_index(value, i);
        u8 byte = *byte_ptr;
        *integer |= byte << (i*8);
    }

    return SResult_new(NULL);
}

static SResult Data_subscript_helper(in Data* src, in Data* index, in Generator* generator, out Data* data) {
    u64 integ = 0;
    SRESULT_UNWRAP(
        Data_as_integer(index, &integ),
        (void)NULL
    );

    SRESULT_UNWRAP(
        Type_subscript(&src->type, generator, integ, &data->type),
        (void)NULL
    );

    SRESULT_UNWRAP(
        Storage_subscript(&src->storage, integ, data->type.size, &data->storage),
        (void)NULL
    );

    return SResult_new(NULL);
}

SResult Data_subscript(Data src, Data index, in Generator* generator, out Data* data) {
    assert(data != NULL);

    SRESULT_UNWRAP(
        Data_subscript_helper(&src, &index, generator, data),
        Data_free(src);Data_free(index);
    );

    Data_free(src);
    Data_free(index);

    return SResult_new(NULL);
}

Data Data_void(void) {
    Data data = {
        {"void", "", Type_Integer, {}, 0, 1},
        {StorageType_imm, {.imm = {Imm_Value, {.value = Vec_new(sizeof(u8))}}}}
    };

    return data;
}

SResult Data_ref(Data src, in Generator* generator, out Data* data) {
    assert(generator != NULL);
    assert(data != NULL);

    SRESULT_UNWRAP(
        Type_ref(&src.type, generator, &data->type),
        Data_free(src)
    );

    Storage* src_storage = &src.storage;
    Storage* data_storage = &data->storage;
    switch(src_storage->type) {
        case StorageType_reg:
            *data_storage = Storage_refer_reg(src_storage->body.reg);
            break;
        default:
            Data_free(src);
            return SResult_new("expected register");
    }

    Data_free(src);

    return SResult_new(NULL);
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
    Storage_free(self.storage);
}

void Data_free_for_vec(inout void* ptr) {
    Data* data = ptr;
    Data_free(*data);
}
