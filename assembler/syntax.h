#pragma once
#include "types.h"
#include "gen.h"

typedef struct {
    Register reg;
    i32 stack_offset;
} StoredReg;

typedef struct {
    i32 stack_offset;
    u32 variables_base;
    Vec stored_regs;// Vec<StoredReg>
} VariableBlock;

typedef struct {
    Vec variables;
    i32 stack_offset;
    Vec blocks;// Vec<VariableBlock>
} VariableManager;

void StoredReg_print(in StoredReg* self);
void StoredReg_print_for_vec(in void* ptr);

void VariableBlock_print(in VariableBlock* self);
void VariableBlock_print_for_vec(in void* ptr);
void VariableBlock_free(VariableBlock self);
void VariableBlock_free_for_vec(inout void* ptr);

VariableManager VariableManager_new(i32 stack_offset);
i32* VariableManager_stack_offset(in VariableManager* self);
SResult VariableManager_push(inout VariableManager* self, Variable variable, inout Generator* generator);
void VariableManager_push_alias(inout VariableManager* self, Variable variable);
SResult VariableManager_get(inout VariableManager* self, in char* name, out Variable* variable);
void VariableManager_new_block(inout VariableManager* self);
SResult VariableManager_delete_block(inout VariableManager* self, inout Generator* generator);
SResult VariableManager_delete_allblock(inout VariableManager* self, inout Generator* generator);
void VariableManager_print(in VariableManager* self);
void VariableManager_print_for_vec(in void* ptr);
void VariableManager_free(VariableManager self);
void VariableManager_free_for_vec(inout void* ptr);

void SyntaxTree_build(Parser parser, inout Generator* generator, inout VariableManager* variable_manager);

void check_parser(in Parser* parser, inout Generator* generator);
SResult expand_asmacro(in char* name, in char* path, Vec arguments, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
SResult Syntax_build(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_asmacro_expansion(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_call_fnptr(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_variable_declaration(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_static_variable_declaration(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_const_variable_declaration(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_register_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_imm_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_true_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_false_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_null_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_char_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_assignment(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_dot_operator(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_refer_operator(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_subscript_operator(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_enum_expr(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_sizeof_operator(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_alignof_operator(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_variable_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_if(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_for(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_while(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_block(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_paren(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_assignadd(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_assignsub(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_assignand(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_assignor(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_assignxor(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_assignlea(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_implicit_static_string(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_inc(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_dec(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_debug_label_syntax(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
bool Syntax_build_return(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);

