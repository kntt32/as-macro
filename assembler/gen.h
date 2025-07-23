#pragma once
#include "types.h"
#include "register.h"
#include "vec.h"

#define REX_B 0x1
#define REX_X 0x2
#define REX_R 0x4
#define REX_W 0x8

struct Type;

struct Type {
    char name[256];
    char valid_path[256];
    enum {
        Type_Integer,
        Type_Ptr,
        Type_Array,
        Type_Struct,
        Type_Enum,
        Type_Union,
        Type_Floating,
        Type_LazyPtr,
        Type_Alias,
        Type_Fn,
    } type;
    union {
        struct Type* t_ptr;
        struct { struct Type* type; u32 len; } t_array;
        Vec t_struct;// Vec<StructMember>
        Vec t_enum;// Vec<EnumMember>
        Vec t_union;// Vec<UnionMember>
        struct {char name[256]; char path[256];} t_lazy_ptr;
        struct Type* t_alias;
        Vec t_fn;// Vec<Data>
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
    u32 value;
} EnumMember;

typedef StructMember UnionMember;

typedef struct {
    u8 reg;// 0, 1, 2, 4, 8
    u8 regmem;// 0, 1, 2, 4, 8
} AsmArgSize;

typedef struct {
    enum {
        ModRmType_R,
        ModRmType_Digit,
    } type;
    union {
        u8 digit;// 0~7
        u8 r;// 0, 1, 2, 4, 8 size of reg field register
    } body;
    u8 memsize;// 0, 1, 2, 4, 8 size of regmem field operand
} ModRmType;

typedef struct {
    enum {
        AsmEncodingElement_Value,
        AsmEncodingElement_Imm,
        AsmEncodingElement_ModRm,
        AsmEncodingElement_AddReg,
    } type;
    union {
        u8 value;
        u8 imm;// size of immidiate value (1, 2, 4, 8)
        ModRmType mod_rm;// modrm type
        u8 add_reg;// register size to add (1, 2, 4, 8)
    } body;
} AsmEncodingElement;

typedef struct {
    Vec encoding_elements;// Vec<AsmEncodingElement>
    u8 default_operand_size;// 4, 8
    u8 operand_size;// 1, 2, 4, 8
} AsmEncoding;

typedef enum {
    StorageType_reg,
    StorageType_mem,
    StorageType_xmm,
    StorageType_imm
} StorageType;

typedef struct {
    enum { Index_None, Index_Reg } type;
    union { Register reg; } body;
    u8 scale;
} Index;

typedef struct {
    i32 offset;
    char label[256];
} Disp;

typedef struct {
    Register base;
    Disp disp;
} Memory;

typedef struct {
    enum { Imm_Value, Imm_Label} type;
    union {
        Vec value;// Vec<u8>
        char label[256];
    } body;
} Imm;

typedef struct {
    StorageType type;
    union {
        Register reg;
        Memory mem;
        Register xmm;
        Imm imm;
    } body;
} Storage;

typedef struct {
    Type type;
    Storage storage;
} Data;

typedef struct {
    char name[256];
    char valid_path[256];
    Data data;
    bool defer_flag;
} Variable;

typedef struct {
    char name[256];
    Type type;
    enum { Argument_Trait, Argument_Storage } storage_type;
    union {
        struct {
            bool reg_flag;
            bool mem_flag;
            bool imm_flag;
            bool xmm_flag;
        } trait;
        Storage storage;
    } storage;
} Argument;

typedef struct {
    bool reg_flag;
    enum {AsmArgs_Reg_Reg, AsmArgs_Reg_Xmm} reg_type;
    union {
        Register reg;
        Register xmm;
    } reg;
    bool regmem_flag;
    enum {AsmArgs_Rm_Reg, AsmArgs_Rm_Mem, AsmArgs_Rm_Xmm} regmem_type;
    union {
        Register reg;
        Memory mem;
        Register xmm;
    } regmem;
    bool imm_flag;
    optional Imm imm;
} AsmArgs;

typedef struct {
    char name[256];
    char valid_path[256];// public: \0

    Vec arguments;// Vec<Argument>
    enum { Asmacro_AsmMacro, Asmacro_UserMacro, Asmacro_FnWrapper, Asmacro_FnExtern } type;
    union {
        AsmEncoding asm_macro;
        Parser user_macro;
        Vec fn_wrapper;// Vec<Variable>
        Vec fn_extern;// Vec<Variable>
    } body;
} Asmacro;

typedef struct {
    char name[256];
    Vec binary;// Vec<u8>
} Section;

typedef enum {
    Label_Func,
    Label_Notype,
    Label_Object
} LabelType;

typedef struct {
    pub char name[256];
    pub bool public_flag;
    pub char section_name[256];
    pub u32 offset;
    pub LabelType type;
    pub u32 size;
} Label;

typedef struct {
    char label[256];
    char section_name[256];
    u32 offset;
    i32 addend;
    bool flag;
} Rela;

typedef struct {
    char path[4096];
    char* src;
} Import;

typedef struct {
    char name[256];
    char valid_path[256];
    Vec vars;// Vec<char[256]>
    Parser parser;
    Vec parser_vars;// Vec<Vec<ParserVar>*>
} Template;

typedef struct {
    Offset offset;
    char msg[256];
} Error;

typedef struct {
    Vec types;// Vec<Type>
    Vec global_variables;// Vec<Variable>
    Vec asmacroes;// Vec<Asmacro>
    Vec templates;// Vec<Template>
    Vec errors;// Vec<Error>
    Vec imports;// Vec<Import>
    Vec import_paths;// Vec<char*>
    Vec expand_stack;// Vec<Asmacro>

    char namespace[256];
    Vec sections;// Vec<Section>
    Vec labels;// Vec<Label>
    Vec relas;// Vec<Rela>
} Generator;

Vec Type_primitives(void);
ParserMsg Type_parse_struct(inout Parser* parser, in Generator* generator, out Type* type);
ParserMsg Type_parse_enum(inout Parser* parser, out Type* type);
ParserMsg Type_parse_union(inout Parser* parser, in Generator* generator, out Type* type);
ParserMsg Type_parse_lazyptr(inout Parser* parser, out Type* type);
ParserMsg Type_parse_fnptr(inout Parser* parser, in Generator* generator, out Type* type);
ParserMsg Type_parse(inout Parser* parser, in Generator* generator, out Type* type);
ParserMsg Type_get_enum_member(Type self, Parser parser, out Data* data);
ParserMsg Type_initialize(in Type* self, inout Parser* parser, inout Vec* bin);
Type Type_clone(in Type* self);
void Type_restrict_namespace(inout Type* self, in char* namespace);
Type Type_as_alias(Type self, in char* name);
Type Type_fn_from(in Vec* arguments);
SResult Type_dot_element(in Type* self, in char* element, out u32* offset, out Type* type);
SResult Type_ref(in Type* src, in Generator* generator, out Type* type);
SResult Type_subscript(in Type* self, in Generator* generator, u32 index, out Type* type);
bool Type_cmp(in Type* self, in Type* other);
bool Type_match(in Type* self, in Type* other);
Type Type_void(void);
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

SResult AsmArgSize_from(in Vec* arguments, out AsmArgSize* sizes);

ParserMsg ModRmType_parse(inout Parser* parser, in AsmArgSize* sizes, out ModRmType* mod_rm_type);
SResult ModRmType_encode_rex_prefix(in ModRmType* self, in AsmArgs* args, inout u8* rex_prefix, inout bool* rex_prefix_needed_flag);
void ModRmType_print(in ModRmType* self);

ParserMsg AsmEncodingElement_parse(inout Parser* parser, in AsmArgSize* sizes, out AsmEncodingElement* asm_encoding_element);
SResult AsmEncodingElement_encode_rexprefix(in AsmEncodingElement* self, in AsmArgs* args, inout u8* rex_prefix, inout bool* rex_prefix_need_flag);
void AsmEncodingElement_print(in AsmEncodingElement* self);
void AsmEncodingElement_print_for_vec(in void* ptr);

ParserMsg AsmEncoding_parse(inout Parser* parser, in AsmArgSize* sizes, out AsmEncoding* asm_encoding);
SResult AsmEncoding_encode(in AsmEncoding* self, in AsmArgs* args, inout Generator* generator);
void AsmEncoding_print(in AsmEncoding* self);
AsmEncoding AsmEncoding_clone(in AsmEncoding* self);
void AsmEncoding_free(AsmEncoding self);

u64 Disp_size(in Disp* self);
i32 Disp_value(in Disp* self);
SResult Disp_set_label(in Disp* self, inout Generator* generator);
void Disp_print(in Disp* self);

bool Memory_cmp(in Memory* self, in Memory* other);
void Memory_print(in Memory* self);

void Imm_print(in Imm* self);
bool Imm_cmp(in Imm* self, in Imm* other);
Imm Imm_clone(in Imm* self);
void Imm_free(Imm self);

ParserMsg Storage_parse(inout Parser* parser, inout i32* stack_offset, in Type* type, out Storage* storage);
SResult Storage_add_offset(inout Storage* self, i32 offset);
SResult Storage_replace_label(inout Storage* self, in char* label);
Storage Storage_refer_reg(Register reg);
SResult Storage_subscript(in Storage* self, u32 index, u32 size, out Storage* storage);
bool Storage_cmp(in Storage* self, in Storage* other);
void Storage_print(in Storage* self);
Storage Storage_clone(in Storage* self);
void Storage_free(Storage self);

ParserMsg Data_parse(inout Parser* parser, in Generator* generator, inout i32* stack_offset, out Data* data);
bool Data_cmp(in Data* self, in Data* other);
bool Data_cmp_for_vec(in void* left, in void* right);
bool Data_cmp_signature(in Data* self, in Data* other);
bool Data_cmp_signature_for_vec(in void* left, in void* right);
Data Data_from_register(Register reg);
Data Data_from_imm(u64 imm);
Data Data_from_imm_bin(Vec bin, Type type);
Data Data_from_label(in char* label);
Data Data_from_mem(Register reg, i32 offset, in optional char* label, Type type);
Data Data_true(void);
Data Data_false(void);
Data Data_null(void);
Data Data_from_char(u8 code);
Data Data_clone(in Data* self);
void Data_clone_for_vec(in void* src, out void* dst);
SResult Data_dot_operator(in Data* left, in char* element, out Data* data);
SResult Data_ref(Data src, in Generator* generator, out Data* data);
SResult Data_as_integer(in Data* self, out u64* integer);
SResult Data_subscript(Data src, Data index, in Generator* generator, out Data* data);
Data Data_void(void);
void Data_print(in Data* self);
void Data_print_for_vec(in void* ptr);
void Data_free(Data self);
void Data_free_for_vec(inout void* ptr);

Variable Variable_new(in char* name, in char* valid_path, Data data, bool defer_flag);
ParserMsg Variable_parse(inout Parser* parser, in Generator* generator, inout i32* rbp_offset, out Variable* variable);
ParserMsg Variable_parse_static(inout Parser* parser, bool public_flag, bool static_flag, inout Generator* generator);
ParserMsg Variable_parse_static_local(inout Parser* parser, in Generator* generator, out Variable* variable);
ParserMsg Variable_parse_const(inout Parser* parser, bool public_flag, inout Generator* generator);
ParserMsg Variable_parse_const_local(inout Parser* parser, inout Generator* generator, out Variable* variable);
Variable Variable_from_function(in char* name, in char* valid_path, in Vec* arguments);
void Variable_restrict_namespace(inout Variable* self, in char* namespace);
bool Variable_cmp(in Variable* self, in Variable* other);
bool Variable_cmp_for_vec(in void* left, in void* right);
Variable Variable_clone(in Variable* self);
void Variable_clone_for_vec(out void* dst, in void* src);
SResult Variable_get_stack_offset(in Variable* self, out i32* rbp_offset);
SResult Variable_set_stack_offset(inout Variable* self, i32 rbp_offset);
Type* Variable_get_type(in Variable* self);
Storage* Variable_get_storage(in Variable* self);
Vec Variables_from_datas(in Vec* datas);
void Variable_print(in Variable* self);
void Variable_print_for_vec(in void* ptr);
void Variable_free(Variable self);
void Variable_free_for_vec(inout void* ptr);

ParserMsg Argument_parse(inout Parser* parser, in Generator* generator, out Argument* argument);
bool Argument_cmp(in Argument* self, out Argument* other);
bool Argument_cmp_for_vec(in void* self, in void* other);
Argument Argument_from(in Variable* variable);
bool Argument_match_with(in Argument* self, in Data* data);
void Argument_print(in Argument* self);
Argument Argument_clone(in Argument* self);
void Argument_clone_for_vec(in void* src, out void* dst);
void Argument_free(Argument self);
void Argument_free_for_vec(inout void* ptr);

SResult AsmArgs_from(in Vec* dataes, in Vec* arguments, out AsmArgs* asm_args);
AsmArgs AsmArgs_clone(in AsmArgs* self);
void AsmArgs_free(AsmArgs self);

ParserMsg Asmacro_parse(inout Parser* parser, in Generator* generator, out Asmacro* asmacro);
bool Asmacro_cmp_signature(in Asmacro* self, in Asmacro* other);
Asmacro Asmacro_new_fn_wrapper(in char* name, Vec arguments/* Vec<Variable> */, in char* valid_path);
Asmacro Asmacro_new_fn_extern(in char* name, Vec arguments/* Vec<Variable> */, in char* valid_path);
SResult Asmacro_match_with(in Asmacro* self, in Vec* dataes, in char* path);
void Asmacro_print(in Asmacro* self);
void Asmacro_print_for_vec(in void* ptr);
bool Asmacro_cmp(in Asmacro* self, in Asmacro* other);
Asmacro Asmacro_clone(in Asmacro* self);
void Asmacro_free(Asmacro self);
void Asmacro_free_for_vec(inout void* ptr);

SResult Section_new(in char* name, out Section* section);
void Section_print(in Section* self);
void Section_print_for_vec(in void* ptr);
void Section_free(Section self);
void Section_free_for_vec(inout void* ptr);

void LabelType_print(LabelType type);

void Label_print(in Label* self);
void Label_print_for_vec(in void* ptr);

void Rela_print(in Rela* self);
void Rela_print_for_vec(in void* ptr);

void Import_print(in Import* self);
void Import_print_for_vec(in void* ptr);
void Import_free(Import self);
void Import_free_for_vec(inout void* ptr);

ParserMsg Template_parse(inout Parser* parser, out Template* template);
SResult Template_expand(inout Template* self, Vec parser_vars, out bool* expanded_flag, out Parser* template_parser);
void Template_print(in Template* self);
void Template_print_for_vec(in void* self);
Template Template_clone(in Template* self);
void Template_free(Template self);
void Template_free_for_vec(inout void* ptr);

Error Error_new(Offset offset, in char* msg);
Error Error_from_parsermsg(ParserMsg parser_msg);
Error Error_from_sresult(Offset offset, SResult result);
void Error_print(in Error* self);
void Error_print_for_vec(in void* ptr);

Generator Generator_new(Vec import_paths);
SResult Generator_import_by_path(inout Generator* self, in char* path, out bool* doubling_flag, out Parser* optional parser);
ParserMsg Generator_import(inout Generator* self, Parser parser, out bool* doubling_flag, out Parser* optional import_parser);
SResult Generator_add_type(inout Generator* self, Type type);
SResult Generator_get_type(in Generator* self, in char* name, in char* path, out Type* type);
SResult Generator_add_global_variable(inout Generator* self, Variable variable);
SResult Generator_get_global_variable(in Generator* self, in char* path, in char* name, out Variable* variable);
SResult Generator_add_asmacro(inout Generator* self, Asmacro asmacro);
SResult Generator_get_asmacro(in Generator* self, in char* name, out Vec* asmacroes);
void Generator_add_error(inout Generator* self, Error error);
SResult Generator_new_section(inout Generator* self, in char* name);
SResult Generator_append_label(inout Generator* self, optional in char* section, in char* name, bool global_flag, LabelType type);
SResult Generator_end_label(inout Generator* self, in char* name);
SResult Generator_append_rela(inout Generator* self, in char* section, in char* label, bool flag);
SResult Generator_addend_rela(inout Generator* self, in char* section);
SResult Generator_append_binary(inout Generator* self, in char* name, u8 byte);
SResult Generator_last_binary(in Generator* self, in char* name, out u8** byte);
SResult Generator_binary_len(in Generator* self, in char* name, out u32* len);
SResult Generator_asmacro_expand_check(inout Generator* self, Asmacro asmacro);
void Generator_finish_asmacro_expand(inout Generator* self);
SResult Generator_add_template(inout Generator* self, Template template);
SResult Generator_expand_template(
    inout Generator* self, in char* name, in char* path, Vec parser_vars, out bool* expanded_flag, out Parser* template_parser);
bool Generator_is_error(in Generator* self);
void Generator_print(in Generator* self);
void Generator_free(Generator self);
bool resolve_sresult(SResult result, Offset offset, inout Generator* generator);
bool resolve_parsermsg(ParserMsg msg, inout Generator* generator);


