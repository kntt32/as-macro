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
    } type;
    union {
        struct Type* t_ptr;
    } body;
    u32 size;
    u64 align;
};

typedef struct Type Type;

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
    Vec types;// Vec<Type>
} Generator;


ParserMsg Type_parse(inout Parser* parser, in Generator* generator, out Type* type);
Type Type_clone(in Type* self);
void Type_print(in Type* self);
void Type_print_for_vec(void* ptr);
void Type_free(Type self);
void Type_free_for_vec(void* ptr);

void Memory_print(in Memory* self);

ParserMsg Data_parse(inout Parser* parser, in Generator* generator, i32 rbp_offset, out Data* data);
ParserMsg Storage_parse(inout Parser* parser, i32 rbp_offset, out Storage* storage);
void Storage_print(in Storage* self);

void Data_print(in Data* self);
void Data_free(Data self);

Generator Generator_new();
SResult Generator_get_type(in Generator* self, in char* name, out Type* type);
void Generator_print(in Generator* self);
void Generator_free(Generator self);


