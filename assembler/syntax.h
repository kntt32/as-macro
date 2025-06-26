#pragma once
#include "types.h"
#include "gen.h"

typedef struct {
    Vec variables;
    i32 stack_offset;
} VariableManager;

typedef struct {
    bool ok_flag;
    u32 line;
    enum {
        GlobalSyntax_StructDefinision,
        GlobalSyntax_EnumDefinision,
        GlobalSyntax_AsmacroDefinision,
        GlobalSyntax_TypeAlias,
        GlobalSyntax_FunctionDefinision
    } type;
    union {
        Asmacro asmacro_definision;
        struct { char name[256]; Parser proc_parser; VariableManager variable_manager; } function_definision;
    } body;
} GlobalSyntax;

typedef struct {
    Generator generator;
    Vec global_syntaxes;// Vec<GlobalSyntax>
} GlobalSyntaxTree;

VariableManager VariableManager_new(i32 stack_offset);
i32* VariableManager_stack_offset(in VariableManager* self);
void VariableManager_push(inout VariableManager* self, Variable variable);
SResult VariableManager_get(inout VariableManager* self, in char* name, out Variable* variable);
void VariableManager_print(in VariableManager* self);
void VariableManager_print_for_vec(in void* ptr);
VariableManager VariableManager_clone(in VariableManager* self);
void VariableManager_free(VariableManager self);
void VariableManager_free_for_vec(inout void* ptr);

ParserMsg GlobalSyntax_parse(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax);
void GlobalSyntax_check_asmacro(inout GlobalSyntax* self, inout Generator* generator);
void GlobalSyntax_print(in GlobalSyntax* self);
void GlobalSyntax_free(GlobalSyntax self);
void GlobalSyntax_free_for_vec(inout void* ptr);

GlobalSyntaxTree GlobalSyntaxTree_new();
void GlobalSyntaxTree_parse(inout GlobalSyntaxTree* self, Parser parser);
void GlobalSyntaxTree_check_asmacro(inout GlobalSyntaxTree* self);
void GlobalSyntaxTree_print(in GlobalSyntaxTree* self);
void GlobalSyntaxTree_free(GlobalSyntaxTree self);




