#include <stdio.h>
#include <assert.h>
#include "gen.h"

ParserMsg Data_parse(inout Parser* parser, in Generator* generator, inout i32* rbp_offset, out Data* data) {
    assert(parser != NULL && generator != NULL && rbp_offset != NULL && data != NULL);
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
    *rbp_offset += data->type.size;

    *parser = parser_copy;

    return ParserMsg_new(parser->offset, NULL);
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
            {StorageType_xmm, {.reg = reg}}
        };
        return data;
    }
    
    PANIC("unreachable");
    Data uninit;
    return uninit;
}

Data Data_from_imm(u64 imm) {
    Type type;
    u32 len = 0;;
    if(imm <= 0xff) {
        Type tmp = {"i8", Type_Integer, {}, 1, 1};
        type = tmp;
        len = 1;
    }else if(imm <= 0xffff) {
        Type tmp = {"i16", Type_Integer, {}, 2, 2};
        type = tmp;
        len = 2;
    }else if(imm <= 0xffffffff) {
        Type tmp = {"i32", Type_Integer, {}, 4, 4};
        type = tmp;
        len = 4;
    }else {
        Type tmp = {"i64", Type_Integer, {}, 8, 8};
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

Data Data_from_label(in char* label) {
    Data data = {
        {"i32", Type_Integer, {}, 4, 4},
        {StorageType_imm, {.imm = {Imm_Label, {.label = ""}}}}
    };
    strncpy(data.storage.body.imm.body.label, label, 255);

    return data;
}

Data Data_from_mem(Register reg, i32 offset) {
    Data data = {
        {"i32", Type_Integer, {}, 4, 4},
        {StorageType_mem, {.mem = {reg, {Disp_Offset, {.offset = offset}}}}}
    };
    return data;
}

Data Data_clone(in Data* self) {
    Data data;
    
    data.type = Type_clone(&self->type);
    data.storage = Storage_clone(&self->storage);
    
    return data;
}

Data Data_void(void) {
    Data data = {
        {"void", Type_Integer, {}, 0, 1},
        {StorageType_imm, {.imm = {Imm_Value, {.value = Vec_new(sizeof(u8))}}}}
    };

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
    Storage_free(self.storage);
}

void Data_free_for_vec(inout void* ptr) {
    Data* data = ptr;
    Data_free(*data);
}
