#pragma once
#include "types.h"
#include "register.h"
#include "vec.h"
#include "asm.h"

struct Type;

struct Type {
    char name[256];
    enum {
        Type_Integer,
        Type_Ptr,
        Type_Array,
        Type_Struct,
        Type_Enum,
    } type;
    union {
        struct Type* t_ptr;
        struct { struct Type* type; u32 len; } t_array;
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
    char name[256];
    bool type_flag;
    optional Type type;
    bool reg_flag;
    bool mem_flag;
    u32 size;
} Argument;

typedef struct {
    char name[256];
    Vec arguments;// Vec<Argument>
    enum { Asmacro_AsmOperator, Asmacro_UserOperator } type;
    union {
        AsmEncoding asm_operator;
        struct { Parser parser; char* src; } user_operator;
    } body;
} Asmacro;

typedef struct {
    char name[256];
    Vec binary;// Vec<u8>
} Section;

typedef struct {
    pub char name[256];
    pub bool public_flag;
    pub i32 section_index;// <0: unknown
    pub u64 offset;
} Label;

typedef struct {
    u32 line;
    char msg[256];
} Error;

typedef struct {
    Vec types;// Vec<Type>
    Vec global_variables;// Vec<Variable>
    Vec asmacroes;// Vec<Asmacro>
    Vec errors;// Vec<Error>
    
    Vec sections;// Vec<Section>
    Vec labels;// Vec<Label>
} Generator;

ParserMsg Type_parse_struct(inout Parser* parser, in Generator* generator, out Type* type);
ParserMsg Type_parse_enum(inout Parser* parser, out Type* type);
ParserMsg Type_parse(inout Parser* parser, in Generator* generator, out Type* type);
Type Type_clone(in Type* self);
bool Type_cmp(in Type* self, in Type* other);
void Type_print(in Type* self);
void Type_print_for_vec(void* ptr);
void Type_free(Type self);
void Type_free_for_vec(void* ptr);

ParserMsg StructMember_parse(inout Parser* parser, in Generator* generator, inout u64* align, inout u32* size, out StructMember* struct_member);
StructMember StructMember_clone(in StructMember* self);
void StructMember_clone_for_vec(out void* dst, in void* src);
bool StructMember_cmp(in StructMember* self, in StructMember* other);
bool StructMember_cmp_for_vec(in void* self, in void* other);
void StructMember_print(in StructMember* self);
void StructMember_print_for_vec(in void* ptr);
void StructMember_free(StructMember self);
void StructMember_free_for_vec(void* ptr);

ParserMsg EnumMember_parse(inout Parser* parser, inout i32* value, out EnumMember* enum_member);
bool EnumMember_cmp(in EnumMember* self, in EnumMember* other);
bool EnumMember_cmp_for_vec(in void* self, in void* other);
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
void Variable_print_for_vec(in void* ptr);
void Variable_free(Variable self);
void Variable_free(Variable self);

ParserMsg Argument_parse(inout Parser* parser, in Generator* generator, out Argument* argument);
bool Argument_cmp(in Argument* self, out Argument* other);
bool Argument_cmp_for_vec(in void* self, in void* other);
void Argument_print(in Argument* self);
void Argument_free(Argument self);
void Argument_free_for_vec(inout void* ptr);

ParserMsg Asmacro_parse(inout Parser* parser, in Generator* generator, out Asmacro* asmacro);
bool Asmacro_cmp_signature(in Asmacro* self, in Asmacro* other);
void Asmacro_print(in Asmacro* self);
void Asmacro_print_for_vec(in void* ptr);
void Asmacro_free(Asmacro self);
void Asmacro_free_for_vec(inout void* ptr);

SResult Section_new(in char* name, out Section* section);
void Section_print(in Section* self);
void Section_print_for_vec(in void* ptr);
void Section_free(Section self);
void Section_free_for_vec(inout void* ptr);

void Label_print(in Label* self);
void Label_print_for_vec(in void* ptr);

Error Error_from_parsermsg(ParserMsg parser_msg);
Error Error_from_sresult(u32 line, SResult result);
void Error_print(in Error* self);
void Error_print_for_vec(in void* ptr);

Generator Generator_new();
SResult Generator_add_type(inout Generator* self, Type type);
SResult Generator_get_type(in Generator* self, in char* name, out Type* type);
SResult Generator_add_global_variable(inout Generator* self, Variable variable);
SResult Generator_add_asmacro(inout Generator* self, Asmacro asmacro);
void Generator_add_error(inout Generator* self, Error error);
SResult Generator_new_section(inout Generator* self, in char* name);
SResult Generator_append_binary(inout Generator* self, in char* name, u8 byte);
void Generator_print(in Generator* self);
void Generator_free(Generator self);


