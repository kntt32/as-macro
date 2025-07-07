#pragma once

#include <elf.h>
#include "types.h"
#include "vec.h"
#include "gen.h"

typedef struct {
    Vec table;// Vec<char>
} StrTable;

typedef struct {
    Vec bin;// Vec<u8>
} RawBin;

Vec Elf64(in Generator* generator);
Elf64_Ehdr Elf64_Ehdr_new(inout u32* elf_offset);
Elf64_Shdr Elf64_Shdr_null(void);
Elf64_Shdr Elf64_shdr_shstrtab(inout StrTable* self);
void Elf64_Shdr_symtab_and_strtab(inout Vec* shdrs, in Generator* generator, inout StrTable* shstrtable, inout RawBin* rawbin);
Elf64_Shdr Elf64_Shdr_from(in Section* section, inout StrTable* shstrtable, inout RawBin* rawbin);
Elf64_Sym Elf64_Sym_null(void);

StrTable StrTable_new(void);
u32 StrTable_push(inout StrTable* self, in char* str);
void StrTable_resolve(in StrTable* self, inout Vec* shdrs, inout u32* elf_offset);
void StrTable_write(in StrTable* self, inout Vec* binary);
u32 StrTable_size(in StrTable* self);
u32 StrTable_rawbin(in StrTable* self, inout RawBin* rawbin);
char* StrTable_str(in StrTable* self, u32 index);
void StrTable_print(in StrTable* self);
void StrTable_free(StrTable self);

RawBin RawBin_new(void);
u32 RawBin_push(inout RawBin* self, in Vec* bin);
u32 RawBin_push_arr(inout RawBin* self, in void* ptr, u32 size);
void RawBin_resolve(in RawBin* self, in StrTable* shstrtable, inout Vec* shdrs, inout u32* elf_offset);
void RawBin_write(in RawBin* self, inout Vec* binary);
void RawBin_print(in RawBin* self);
void RawBin_free(RawBin self);
