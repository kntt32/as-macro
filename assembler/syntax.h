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

void check_parser(in Parser* parser, inout Generator* generator);
SResult expand_asmacro(in char* name, in char* path, Vec arguments, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
SResult Syntax_build(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_asmacro_expansion(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_variable_declaration(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_register_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_imm_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_assignment(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_assignment(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_variable_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_return(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);

