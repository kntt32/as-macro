#include <stdio.h>
#include <assert.h>
#include "gen.h"

u64 Disp_size(in Disp* self) {
    assert(self != NULL);
    u64 size = 0;
    
    switch(self->type) {
        case Disp_None:
            size = 0;
            break;
        case Disp_Offset:
            size = (self->body.offset < 256)?(8):(32);
            break;
        case Disp_Label:
            size = 8;
            break;
    }

    return size;
}

u64 Disp_value(in Disp* self) {
    assert(self != NULL);
    u64 value = 0;

    switch(self->type) {
        case Disp_None:
            value = 0;
            break;
        case Disp_Offset:
            value = (u32)self->body.offset;
            break;
        case Disp_Label:
            value = 0;
            break;
    }

    return value;
}

SResult Disp_set_label(in Disp* self, inout Generator* generator) {
    if(self->type == Disp_Label) {
        return Generator_append_rela(generator, "text", self->body.label, false);
    }

    return SResult_new(NULL);
}

void Disp_print(in Disp* self) {
    printf("Disp { type: %d, body: ", self->type);
    switch(self->type) {
        case Disp_None:
            printf("none");
            break;
        case Disp_Offset:
            printf(".offset: %d", self->body.offset);
            break;
        case Disp_Label:
            printf(".label: %s", self->body.label);
            break;
    }
    printf(" }");
}

bool Memory_cmp(in Memory* self, in Memory* other) {
    if(self->base != other->base) {
        return false;
    }

    if(self->disp.type != other->disp.type) {
        return false;
    }
    switch(self->disp.type) {
        case Disp_None:
            break;
        case Disp_Offset:
            if(self->disp.body.offset != other->disp.body.offset) {
                return false;
            }
            break;
        case Disp_Label:
            if(strcmp(self->disp.body.label, other->disp.body.label) != 0) {
                return false;
            }
             break;
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
            if(Vec_cmp(&self->body.value, &other->body.value, u8_cmp_for_vec)) {
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

ParserMsg Storage_parse(inout Parser* parser, in i32* rbp_offset, out Storage* storage) {
    if(ParserMsg_is_success(Register_parse(parser, &storage->body.reg))) {
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
        storage->type = StorageType_mem;
        Memory* mem = &storage->body.mem;
        mem->base = Rbp;
        mem->disp.type = Disp_Offset;
        mem->disp.body.offset = *rbp_offset;
    }else {
        return ParserMsg_new(parser->offset, "expected storage");
    }

    return ParserMsg_new(parser->offset, NULL);
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
