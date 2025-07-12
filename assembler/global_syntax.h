#pragma once
#include "syntax.h"

typedef struct {
    bool ok_flag;
    Offset offset;
    enum {
        GlobalSyntax_StructDefinision,
        GlobalSyntax_EnumDefinision,
        GlobalSyntax_AsmacroDefinision,
        GlobalSyntax_TypeAlias,
        GlobalSyntax_FunctionDefinision,
        GlobalSyntax_Import,
    } type;
    union {
        Asmacro asmacro_definision;
        struct { char name[256]; bool public_flag; Parser proc_parser; VariableManager variable_manager; } function_definision;
    } body;
} GlobalSyntax;

typedef struct {
    Generator generator;
    Vec global_syntaxes;// Vec<GlobalSyntax>
} GlobalSyntaxTree;

ParserMsg GlobalSyntax_parse(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax);
void GlobalSyntax_build(inout GlobalSyntax* self, inout Generator* generator);
void GlobalSyntax_print(in GlobalSyntax* self);
void GlobalSyntax_free(GlobalSyntax self);
void GlobalSyntax_free_for_vec(inout void* ptr);

GlobalSyntaxTree GlobalSyntaxTree_new();
void GlobalSyntaxTree_parse(inout GlobalSyntaxTree* self, Parser parser);
void GlobalSyntaxTree_check_asmacro(inout GlobalSyntaxTree* self);
Generator GlobalSyntaxTree_build(inout GlobalSyntaxTree self);
void GlobalSyntaxTree_print(in GlobalSyntaxTree* self);
void GlobalSyntaxTree_free(GlobalSyntaxTree self);
