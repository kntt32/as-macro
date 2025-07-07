#include <stdio.h>
#include <assert.h>
#include <elf.h>
#include "gen_elf.h"

static Vec Elf64_shdrs(in Generator* generator, inout StrTable* shstrtable, inout RawBin* rawbin, inout u32* elf_offset) {
    assert(generator != NULL && shstrtable != NULL && rawbin != NULL);

    Vec shdrs = Vec_new(sizeof(Elf64_Shdr));
    Elf64_Shdr shdr_null = Elf64_Shdr_null();
    Vec_push(&shdrs, &shdr_null);
    Elf64_Shdr shdr_shstrtable = Elf64_shdr_shstrtab(shstrtable);
    Vec_push(&shdrs, &shdr_shstrtable);

    for(u32 i=0; i<Vec_len(&generator->sections); i++) {
        Section* section = Vec_index(&generator->sections, i);
        Elf64_Shdr shdr = Elf64_Shdr_from(section, shstrtable, rawbin);
        Vec_push(&shdrs, &shdr);
    }
    Elf64_Shdr_symtab_and_strtab(&shdrs, generator, shstrtable, rawbin);

    *elf_offset += sizeof(Elf64_Shdr) * Vec_len(&shdrs);

    return shdrs;
}

Vec Elf64_binary(Elf64_Ehdr ehdr, Vec shdrs, StrTable shstrtable, RawBin rawbin) {
    Vec binary = Vec_new(sizeof(u8));
    Vec_append(&binary, &ehdr, sizeof(Elf64_Ehdr));
    
    for(u32 i=0; i<Vec_len(&shdrs); i++) {
        Elf64_Shdr* shdr = Vec_index(&shdrs, i);
        Vec_append(&binary, shdr, sizeof(Elf64_Shdr));
    }
    
    StrTable_write(&shstrtable, &binary);
    RawBin_write(&rawbin, &binary);
    
    Vec_free(shdrs);
    StrTable_free(shstrtable);
    RawBin_free(rawbin);

    return binary;
}

Vec Elf64(in Generator* generator) {
    // Ehdr
    // Shdr
    // StrTable
    // RawBin

    StrTable shstrtable = StrTable_new();
    RawBin rawbin = RawBin_new();

    u32 elf_offset = 0;
    Elf64_Ehdr ehdr = Elf64_Ehdr_new(&elf_offset);
    ehdr.e_shoff = elf_offset;
    Vec shdrs = Elf64_shdrs(generator, &shstrtable, &rawbin, &elf_offset);
    ehdr.e_shnum = Vec_len(&shdrs);

    ehdr.e_shstrndx = 1;
    StrTable_resolve(&shstrtable, &shdrs, &elf_offset);
    RawBin_resolve(&rawbin, &shstrtable, &shdrs, &elf_offset);

    return Elf64_binary(ehdr, shdrs, shstrtable, rawbin);
}

static void Elf64_Ehdr_set_magic(inout Elf64_Ehdr* self) {
    memset(self->e_ident, 0, sizeof(self->e_ident));
    self->e_ident[EI_MAG0] = ELFMAG0;
    self->e_ident[EI_MAG1] = ELFMAG1;
    self->e_ident[EI_MAG2] = ELFMAG2;
    self->e_ident[EI_MAG3] = ELFMAG3;
    self->e_ident[EI_CLASS] = ELFCLASS64;
    self->e_ident[EI_DATA] = ELFDATA2LSB;
    self->e_ident[EI_VERSION] = EV_CURRENT;
    self->e_ident[EI_OSABI] = ELFOSABI_NONE;
    self->e_ident[EI_ABIVERSION] = 0;
}

Elf64_Ehdr Elf64_Ehdr_new(inout u32* elf_offset) {
    Elf64_Ehdr header;
    memset(&header, 0, sizeof(Elf64_Ehdr));

    Elf64_Ehdr_set_magic(&header);
    header.e_type = ET_REL;
    header.e_machine = EM_X86_64;
    header.e_version = EV_CURRENT;
    header.e_entry = 0;
    header.e_phoff = 0;
    header.e_shoff = 0;
    header.e_flags = 0;
    header.e_ehsize = sizeof(Elf64_Ehdr);
    header.e_phentsize = sizeof(Elf64_Phdr);
    header.e_phnum = 0;
    header.e_shentsize = sizeof(Elf64_Shdr);
    header.e_shnum = 0;
    header.e_shstrndx = 0;

    *elf_offset += sizeof(header);

    return header;
}

static u32 find_section(in Vec* shdrs, in StrTable* shstrtable, in char* name) {
    for(u32 i=1; i<Vec_len(shdrs); i++) {
        Elf64_Shdr* shdr = Vec_index(shdrs, i);
        if(strcmp(StrTable_str(shstrtable, shdr->sh_name), name) == 0) {
            return i;
        }
    }

    return 0;
}

Elf64_Shdr Elf64_Shdr_null(void) {
    Elf64_Shdr header;
    memset(&header, 0, sizeof(header));
    return header;
}

Elf64_Shdr Elf64_shdr_shstrtab(inout StrTable* self) {
    Elf64_Shdr header;
    header.sh_name = StrTable_push(self, ".shstrtab");
    header.sh_type = SHT_STRTAB;
    header.sh_flags = 0;
    header.sh_addr = 0;
    header.sh_offset = 0;// resolve after
    header.sh_size = 0;// resolve after
    header.sh_link = 0;
    header.sh_info = 0;
    header.sh_addralign = 1;
    header.sh_entsize = 0;

    return header;
}

Elf64_Sym Elf64_Sym_from(in Label* label, inout char parent_name[512], in Vec* shdrs, in StrTable* shstrtable, inout StrTable* strtable) {
    char name[512];
    if(label->name[0] == '.') {
        snprintf(name, 512, "%.256s%.256s", parent_name, label->name);
    }else {
        strcpy(name, label->name);
        strcpy(parent_name, label->name);
    }

    Elf64_Sym sym;
    sym.st_name = StrTable_push(strtable, name);
    u8 bind = (label->public_flag)?(STB_GLOBAL):(STB_LOCAL);
    u8 type;
    switch(label->type) {
        case Label_Func:
            type = STT_FUNC;
            break;
        case Label_Object:
            type = STT_OBJECT;
            break;
        case Label_Notype:
            type = STT_NOTYPE;
            break;
    }
    sym.st_info = ELF64_ST_INFO(bind, type);
    sym.st_other = 0;
    sym.st_shndx = find_section(shdrs, shstrtable, label->section_name);
    sym.st_value = label->offset;
    sym.st_size = label->size;

    return sym;
}

Elf64_Sym Elf64_Sym_null(void) {
    Elf64_Sym symbol;
    memset(&symbol, 0, sizeof(Elf64_Sym));
    return symbol;
}

static void Elf64_Shdr_symtab_and_strtab_get_sym(
    in Vec* labels, in Vec* shdrs, inout StrTable* shstrtable, inout StrTable* strtable, inout Vec* public_symbols, inout Vec* local_symbols) {
    char parent_symbol[512] = "";

    for(u32 i=0; i<Vec_len(labels); i++) {
        Label* label = Vec_index(labels, i);

        Elf64_Sym symbol = Elf64_Sym_from(label, parent_symbol, shdrs, shstrtable, strtable);
        if(label->public_flag) {
            Vec_push(public_symbols, &symbol);
        }else {
            Vec_push(local_symbols, &symbol);
        }
    }
}

static void Elf64_Shdr_symtab_and_strtab_push_symtab_shdr(inout Vec* shdrs, in Vec* public_symbols, in Vec* local_symbols, inout StrTable* shstrtable, inout RawBin* rawbin) {
    Elf64_Shdr shdr;
    memset(&shdr, 0, sizeof(shdr));
    Elf64_Sym null_symbol = Elf64_Sym_null();
    
    shdr.sh_name = StrTable_push(shstrtable, ".symtab");
    shdr.sh_type = SHT_SYMTAB;
    shdr.sh_flags = 0;
    shdr.sh_addr = 0;
    shdr.sh_offset = RawBin_push_arr(rawbin, &null_symbol, sizeof(null_symbol));
    RawBin_push_arr(rawbin, Vec_as_ptr(local_symbols), Vec_len(local_symbols)*sizeof(Elf64_Sym));
    RawBin_push_arr(rawbin, Vec_as_ptr(public_symbols), Vec_len(public_symbols)*sizeof(Elf64_Sym));
    shdr.sh_size = (1 + Vec_len(local_symbols) + Vec_len(public_symbols))*sizeof(Elf64_Sym);
    shdr.sh_link = Vec_len(shdrs) + 1;
    shdr.sh_info = 1 + Vec_len(local_symbols);
    shdr.sh_addralign = 8;
    shdr.sh_entsize = sizeof(Elf64_Sym);

    Vec_push(shdrs, &shdr);
}

void Elf64_Shdr_symtab_and_strtab_push_strtab_shdr(inout Vec* shdrs, inout StrTable* shstrtable, in StrTable* strtable, inout RawBin* rawbin) {
    Elf64_Shdr shdr;
    memset(&shdr, 0, sizeof(shdr));

    shdr.sh_name = StrTable_push(shstrtable, ".strtab");
    shdr.sh_type = SHT_STRTAB;
    shdr.sh_flags = 0;
    shdr.sh_addr = 0;
    shdr.sh_offset = StrTable_rawbin(strtable, rawbin);
    shdr.sh_size = StrTable_size(strtable);
    shdr.sh_link = 0;
    shdr.sh_info = 0;
    shdr.sh_addralign = 1;
    shdr.sh_entsize = 0;

    Vec_push(shdrs, &shdr);
}

void Elf64_Shdr_symtab_and_strtab(inout Vec* shdrs, in Generator* generator, inout StrTable* shstrtable, inout RawBin* rawbin) {
    assert(shdrs != NULL && Vec_size(shdrs) == sizeof(Elf64_Shdr) && generator != NULL && shstrtable != NULL && rawbin != NULL);
    StrTable strtable = StrTable_new();
    Vec local_symbols = Vec_new(sizeof(Elf64_Sym));
    Vec public_symbols = Vec_new(sizeof(Elf64_Sym));
    
    Elf64_Shdr_symtab_and_strtab_get_sym(&generator->labels, shdrs, shstrtable, &strtable, &public_symbols, &local_symbols);

    Elf64_Shdr_symtab_and_strtab_push_symtab_shdr(shdrs, &public_symbols, &local_symbols, shstrtable, rawbin);
    Elf64_Shdr_symtab_and_strtab_push_strtab_shdr(shdrs, shstrtable, &strtable, rawbin);

    Vec_free(local_symbols);
    Vec_free(public_symbols);
    StrTable_free(strtable);
}

Elf64_Shdr Elf64_Shdr_from(in Section* section, inout StrTable* shstrtable, inout RawBin* rawbin) {
    assert(section != NULL);

    Elf64_Shdr header;
    header.sh_name = StrTable_push(shstrtable, section->name);

    if(strcmp(section->name, ".text") == 0) {
        header.sh_type = SHT_PROGBITS;
        header.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
        header.sh_addr = 0;
        header.sh_offset = RawBin_push(rawbin, &section->binary);
        header.sh_size = Vec_size(&section->binary);
        header.sh_link = 0;
        header.sh_info = 0;
        header.sh_addralign = 0x10;
        header.sh_entsize = 0;
    }else {
        TODO();
    }

    return header;
}

StrTable StrTable_new(void) {
    char null_char = '\0';
    StrTable shstrtable;
    shstrtable.table = Vec_new(sizeof(char));
    Vec_push(&shstrtable.table, &null_char);

    return shstrtable;
}

u32 StrTable_push(inout StrTable* self, in char* str) {
    assert(str != NULL);
    char null_char = '\0';

    u32 index = Vec_len(&self->table);
    for(u32 i=0; i<strlen(str); i++) {
        Vec_push(&self->table, &str[i]);
    }
    Vec_push(&self->table, &null_char);

    return index;
}

void StrTable_resolve(in StrTable* self, inout Vec* shdrs, inout u32* elf_offset) {
    assert(self != NULL && elf_offset != NULL && Vec_size(shdrs) == sizeof(Elf64_Shdr));
    
    for(u32 i=0; i<Vec_len(shdrs); i++) {
        Elf64_Shdr* shdr = Vec_index(shdrs, i);
        if(strcmp(StrTable_str(self, shdr->sh_name), ".shstrtab") == 0) {
            shdr->sh_offset = *elf_offset;
            shdr->sh_size = Vec_len(&self->table);
        }
    }

    *elf_offset += Vec_len(&self->table) * sizeof(u8);
}

void StrTable_write(in StrTable* self, inout Vec* binary) {
    assert(self != NULL && binary != NULL && Vec_size(binary) == sizeof(u8));
    assert(sizeof(u8) == sizeof(char));

    char* table_ptr = Vec_as_ptr(&self->table);
    u32 len = Vec_len(&self->table);
    Vec_append(binary, table_ptr, len);
}

u32 StrTable_size(in StrTable* self) {
    return Vec_len(&self->table) * sizeof(u8);
}

u32 StrTable_rawbin(in StrTable* self, inout RawBin* rawbin) {
    return RawBin_push(rawbin, &self->table);
}

char* StrTable_str(in StrTable* self, u32 index) {
    return Vec_index(&self->table, index);
}

void StrTable_print(in StrTable* self) {
    assert(self != NULL);

    u32 table_len = Vec_len(&self->table);

    printf("StrTable [");
    for(u32 i=0; i<table_len; i++) {
        char* table_char_ptr = Vec_index(&self->table, i);
        char table_char = *table_char_ptr;

        if(table_char == '\0' && i+1 != table_len) {
            printf(", ");
        }else {
            putchar(table_char);
        }
    }
    printf("]");
}

void StrTable_free(StrTable self) {
    Vec_free(self.table);
}

RawBin RawBin_new(void) {
    RawBin self;
    self.bin = Vec_new(sizeof(u8));
    return self;
}

u32 RawBin_push(inout RawBin* self, in Vec* bin) {
    assert(Vec_size(bin) == sizeof(u8));
    
    u32 index = Vec_len(&self->bin);
    for(u32 i=0; i<Vec_len(bin); i++) {
        Vec_push(&self->bin, Vec_index(bin, i));
    }

    return index;
}

u32 RawBin_push_arr(inout RawBin* self, in void* ptr, u32 size) {
    u32 index = Vec_len(&self->bin);
    Vec_append(&self->bin, ptr, size);
    return index;
}

void RawBin_resolve(in RawBin* self, in StrTable* shstrtable, inout Vec* shdrs, inout u32* elf_offset) {
    assert(self != NULL && Vec_size(shdrs) == sizeof(Elf64_Shdr) && elf_offset != NULL);

    for(u32 i=0; i<Vec_len(shdrs); i++) {
        Elf64_Shdr* shdr = Vec_index(shdrs, i);
        if(strcmp(StrTable_str(shstrtable, shdr->sh_name), ".shstrtab") != 0) {
            shdr->sh_offset += *elf_offset;
        }
    }

    *elf_offset += Vec_len(&self->bin) * sizeof(u8);
}

void RawBin_write(in RawBin* self, inout Vec* binary) {
    assert(self != NULL && binary != NULL && Vec_size(binary) == sizeof(u8));

    u8* ptr = Vec_as_ptr(&self->bin);
    u32 len = Vec_len(&self->bin);
    Vec_append(binary, ptr, len);
}

void RawBin_print(in RawBin* self) {
    printf("RawBin [");
    for(u32 i=0; i<Vec_len(&self->bin); i++) {
        if(i != 0) {
            printf(", ");
        }
        u8* byte_ptr = Vec_index(&self->bin, i);
        printf("%x", *byte_ptr);
    }
    printf("]");
}

void RawBin_free(RawBin self) {
    Vec_free(self.bin);
}
