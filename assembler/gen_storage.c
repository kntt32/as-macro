#include <stdio.h>
#include <assert.h>
#include "gen.h"
#include "util.h"

u64 Disp_size(in Disp* self) {
    assert(self != NULL);
    u64 size = 0;
    
    if(self->label[0] != '\0') {
        size = 4;
    }else {
        size = (-128 <= self->offset && self->offset < 128)?(1):(4);
    }

    return size;
}

i32 Disp_value(in Disp* self) {
    assert(self != NULL);
    return self->offset;
}

SResult Disp_set_label(in Disp* self, inout Generator* generator) {
    if(self->label[0] != '\0') {
        return Generator_append_rela(generator, ".text", self->label, false);
    }

    return SResult_new(NULL);
}

void Disp_print(in Disp* self) {
    printf("Disp { offset: %d, label: %s }", self->offset, self->label);
}

bool Memory_cmp(in Memory* self, in Memory* other) {
    if(self->base != other->base) {
        return false;
    } 

    if(self->disp.offset != other->disp.offset || strcmp(self->disp.label, other->disp.label) != 0) {
        return false;
    }
    
    return true;
}

void Memory_print(in Memory* self) {
    printf("Memory { base: ");
    Register_print(self->base);
    printf(", disp: ");
    Disp_print(&self->disp);
    printf(" }");
}

void Imm_print(in Imm* self) {
    printf("Imm { type: %d, body: ", self->type);
    switch(self->type) {
        case Imm_Value:
            Vec_print(&self->body.value, u8_print_for_vec);
            break;
        case Imm_Label:
            printf(".label: %s", self->body.label);
            break;
    }
    printf(" }");
}

bool Imm_cmp(in Imm* self, in Imm* other) {
    if(self->type != other->type) {
        return false;
    }

    switch(self->type) {
        case Imm_Value:
            if(!Vec_cmp(&self->body.value, &other->body.value, u8_cmp_for_vec)) {
                return false;
            }
            break;
        case Imm_Label:
            if(strcmp(self->body.label, other->body.label) != 0) {
                return false;
            }
            break;
    }

    return true;
}

Imm Imm_clone(in Imm* self) {
    assert(self != NULL);

    Imm imm;
    imm.type = self->type;
    switch(self->type) {
        case Imm_Value:
            imm.body.value = Vec_clone(&self->body.value, NULL);
            break;
        case Imm_Label:
            strcpy(imm.body.label, self->body.label);
            break;
    }

    return imm;
}

void Imm_free(Imm self) {
    switch(self.type) {
        case Imm_Value:
            Vec_free(self.body.value);
            break;
        case Imm_Label:
            break;
    }
}

ParserMsg Storage_parse(inout Parser* parser, inout i32* stack_offset, in Type* type, out Storage* storage) {
    if(ParserMsg_is_success(Register_parse(parser, &storage->body.reg))) {
        if(type->type == Type_Array || type->type == Type_Struct) {
            return ParserMsg_new(parser->offset, "array of struct can't store on register or xmm'");
        }
        if(Register_is_integer(storage->body.reg)) {
            storage->type = StorageType_reg;
        }else if(Register_is_xmm(storage->body.reg)) {
            storage->type = StorageType_xmm;
            storage->body.xmm = storage->body.reg;
        }
    }else if(ParserMsg_is_success(Parser_parse_keyword(parser, "imm"))){
        storage->type = StorageType_imm;
        storage->body.imm.type = Imm_Value;
        storage->body.imm.body.value = Vec_new(sizeof(u8));
    }else if(ParserMsg_is_success(Parser_parse_keyword(parser, "stack"))) {
        if(stack_offset == NULL) {
            return ParserMsg_new(parser->offset, "storage \"stack\" can not be used here");
        }else {
            *stack_offset -= type->size;
            *stack_offset = ceil_div(*stack_offset - (i32)type->align + 1, type->align)*type->align;
            storage->type = StorageType_mem;
            Memory* mem = &storage->body.mem;
            mem->base = Rbp;
            mem->disp.label[0] = '\0';
            mem->disp.offset = *stack_offset;
        }
    }else {
        return ParserMsg_new(parser->offset, "expected storage");
    }

    return ParserMsg_new(parser->offset, NULL);
}

SResult Storage_add_offset(inout Storage* self, i32 offset) {
    if(self->type != StorageType_mem) {
        return SResult_new("expected memory storage");
    }

    Disp* disp = &self->body.mem.disp;
    disp->offset += offset;

    return SResult_new(NULL);
}

SResult Storage_replace_label(inout Storage* self, in char* label) {
    if(self->type != StorageType_mem) {
        return SResult_new("expected memory storage");
    }

    snprintf(self->body.mem.disp.label, 256, "%.255s", label);
    return SResult_new(NULL);
}

Storage Storage_refer_reg(Register reg) {
    Storage storage = {
        StorageType_mem,
        {.mem = {reg, {0, ""}}}
    };

    return storage;
}

static SResult Storage_subscript_imm(in Storage* self, u32 index, u32 size, out Storage* storage) {
    assert(self->type == StorageType_imm);

    Imm* self_imm = &self->body.imm;
    if(self_imm->type != Imm_Value) {
        return SResult_new("expected imm");
    }

    Imm imm;
    imm.type = Imm_Value;
    imm.body.value = Vec_new(sizeof(u8));

    if(Vec_len(&self_imm->body.value) <= size*(index + 1)) {
        return SResult_new("out of range");
    }
    for(u32 i=0; i<size; i++) {
        u8* byte_ptr = Vec_index(&self_imm->body.value, size*index + i);
        Vec_push(&imm.body.value, byte_ptr);
    }

    storage->type = StorageType_imm;
    storage->body.imm = imm;

    return SResult_new(NULL);
}

static SResult Storage_subscript_mem(in Storage* self, u32 index, u32 size, out Storage* storage) {
    assert(self->type == StorageType_mem);

    *storage = Storage_clone(self);
    assert(
        SResult_is_success(
            Storage_add_offset(storage, index*size)
        )
    );

    return SResult_new(NULL);
}

SResult Storage_subscript(in Storage* self, u32 index, u32 size, out Storage* storage) {
    assert(self != NULL && storage != NULL);

    switch(self->type) {
        case StorageType_imm:
            SRESULT_UNWRAP(
                Storage_subscript_imm(self, index, size, storage),
                (void)NULL
            );
            break;
        case StorageType_mem:
            SRESULT_UNWRAP(
                Storage_subscript_mem(self, index, size, storage),
                (void)NULL
            );
            break;
        default:
            return SResult_new("expected imm or memory");
    }

    return SResult_new(NULL);
}

bool Storage_cmp(in Storage* self, in Storage* other) {
    if(self->type != other->type) {
        return false;
    }
    switch(self->type) {
        case StorageType_reg:
            return self->body.reg == other->body.reg;
        case StorageType_mem:
            return Memory_cmp(&self->body.mem, &other->body.mem);
        case StorageType_xmm:
            return self->body.xmm == other->body.xmm;
        case StorageType_imm:
            return Imm_cmp(&self->body.imm, &other->body.imm);
    }
    return false;
}

bool Storage_is_depend_on(in Storage* self, in Storage* other) {
    if(self->type != StorageType_mem || other->type != StorageType_reg) {
        return false;
    }
    if(self->body.mem.base == R8) {
        int a = 5;
        a = 3;
    }
    
    return self->body.mem.base == other->body.reg;
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
        case StorageType_xmm:
            printf(".xmm: ");
            Register_print(self->body.xmm);
            break;
        case StorageType_imm:
            printf(".imm: ");
            Imm_print(&self->body.imm);
            break;
    }
    printf(" }");
}

Storage Storage_clone(in Storage* self) {
    Storage storage;

    storage.type = self->type;
    switch(self->type) {
        case StorageType_reg:
        case StorageType_mem:
        case StorageType_xmm:
            storage.body = self->body;
            break;
        case StorageType_imm:
            storage.body.imm = Imm_clone(&self->body.imm);
            break;
    }

    return storage;
}

void Storage_free(Storage self) {
    switch(self.type) {
        case StorageType_reg:
        case StorageType_mem:
        case StorageType_xmm:
            break;
        case StorageType_imm:
            Imm_free(self.body.imm);
            break;
    }
}
