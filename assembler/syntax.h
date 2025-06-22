#pragma once
#include "types.h"
#include "gen.h"

typedef struct {
    Vec variables;
    i32 stack_offset;
} VariableManager;

VariableManager VariableManager_new(i32 stack_offset);
i32* VariableManager_stack_offset(in VariableManager* self);
void VariableManager_push(inout VariableManager* self, Variable variable);
SResult VariableManager_get(inout VariableManager* self, in char* name, out Variable* variable);
void VariableManager_print(in VariableManager* self);
void VariableManager_print_for_vec(in void* ptr);
VariableManager VariableManager_clone(in VariableManager* self);
void VariableManager_free(VariableManager self);
void VariableManager_free_for_vec(inout void* ptr);

Generator GlobalSyntax_build(Parser parser);
bool GlobalSyntax_build_struct(Parser parser, inout Generator* generator);
bool GlobalSyntax_build_enum(Parser parser, inout Generator* generator);
bool GlobalSyntax_build_define_asmacro(Parser parser, inout Generator* generator);
ParserMsg GlobalSyntax_build_type_parse(Parser parser, inout Generator* generator, out Type* type);
bool GlobalSyntax_build_function_definision(Parser parser, inout Generator* generator);

