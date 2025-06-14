#pragma once
#include "types.h"
#include "register.h"
#include "vec.h"

struct Type;

struct Type {
    char name[256];
    enum {
        Type_Integer,
        Type_Ptr,
        Type_Struct,
        Type_Enum,
    } type;
    union {
        struct Type* t_ptr;
        Vec t_struct;// Vec<StructMember>
        Vec t_enum;// Vec<EnumMember>
    } body;
    u32 size;
    u64 align;
};

typedef struct Type Type;

typedef struct {
    char name[256];
    Type type;
    u32 offset;
} StructMember;

typedef struct {
    char name[256];
    i32 value;
} EnumMember;

typedef enum {
    StorageType_reg,
    StorageType_mem
} StorageType;

typedef struct {
    bool base_flag;
    Register base;
    bool index_flag;
    Register index;
    u8 scale;
    bool disp_flag;
    i32 disp;
} Memory;

typedef struct {
    StorageType type;
    union {
        Register reg;
        Memory mem;
    } body;
} Storage;

typedef struct {
    Type type;
    Storage storage;
} Data;

typedef struct {
    char name[256];
    Data data;
} Variable;

typedef struct {
    Vec types;// Vec<Type>
} Generator;


ParserMsg Type_parse_struct(inout Parser* parser, in Generator* generator, out Type* type);
ParserMsg Type_parse_enum(inout Parser* parser, out Type* type);
ParserMsg Type_parse(inout Parser* parser, in Generator* generator, out Type* type);
Type Type_clone(in Type* self);
void Type_print(in Type* self);
void Type_print_for_vec(void* ptr);
void Type_free(Type self);
void Type_free_for_vec(void* ptr);

ParserMsg StructMember_parse(inout Parser* parser, in Generator* generator, inout u64* align, inout u32* size, out StructMember* struct_member);
StructMember StructMember_clone(in StructMember* self);
void StructMember_clone_for_vec(out void* dst, in void* src);
void StructMember_print(in StructMember* self);
void StructMember_print_for_vec(in void* ptr);
void StructMember_free(StructMember self);
void StructMember_free_for_vec(void* ptr);

ParserMsg EnumMember_parse(inout Parser* parser, inout i32* value, out EnumMember* enum_member);
void EnumMember_print(in EnumMember* self);
void EnumMember_print_for_vec(in void* ptr);

void Memory_print(in Memory* self);

ParserMsg Data_parse(inout Parser* parser, in Generator* generator, i32 rbp_offset, out Data* data);
Data Data_clone(in Data* self);
void Data_print(in Data* self);
void Data_free(Data self);

ParserMsg Storage_parse(inout Parser* parser, i32 rbp_offset, out Storage* storage);
void Storage_print(in Storage* self);

ParserMsg Variable_parse(inout Parser* parser, in Generator* generator, i32 rbp_offset, out Variable* variable);
Variable Variable_clone(in Variable* self);
void Variable_print(in Variable* self);
void Variable_free(Variable self);

Generator Generator_new();
SResult Generator_get_type(in Generator* self, in char* name, out Type* type);
void Generator_print(in Generator* self);
void Generator_free(Generator self);


