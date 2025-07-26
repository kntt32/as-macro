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
        GlobalSyntax_FunctionExtern,
        GlobalSyntax_Import,
        GlobalSyntax_StaticVariable,
        GlobalSyntax_ConstVariable,
        GlobalSyntax_TemplateDefinision,
        GlobalSyntax_ImplDeclaration
    } type;
    union {
        Asmacro asmacro_definision;
        struct { char name[256]; bool global_flag; Parser proc_parser; VariableManager variable_manager; } function_definision;
        struct {
            Variable variable;
            Parser init_parser;
            bool static_flag;
        } static_variable;
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

GlobalSyntaxTree GlobalSyntaxTree_new(Vec import_paths);
SResult GlobalSyntaxTree_parse(inout GlobalSyntaxTree* self, in char* path);
Generator GlobalSyntaxTree_build(inout GlobalSyntaxTree self);
void GlobalSyntaxTree_print(in GlobalSyntaxTree* self);
void GlobalSyntaxTree_free(GlobalSyntaxTree self);

