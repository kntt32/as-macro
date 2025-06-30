#include <stdio.h>
#include <stdbool.h>
#include "types.h"
#include "gen.h"
#include "syntax.h"

VariableManager VariableManager_new(i32 stack_offset) {
    VariableManager variable_manager = {Vec_new(sizeof(Variable)), stack_offset};
    return variable_manager;
}

i32* VariableManager_stack_offset(in VariableManager* self) {
    return &self->stack_offset;
}

void VariableManager_push(inout VariableManager* self, Variable variable) {
    for(u32 i=0; i<Vec_len(&self->variables); i++) {
        Variable* ptr = Vec_index(&self->variables, i);
        if(strcmp(ptr->name, variable.name) == 0 || Storage_cmp(&ptr->data.storage, &variable.data.storage)) {
            Variable removed_var;
            Vec_remove(&self->variables, i, &removed_var);
            Variable_free(removed_var);
            i--;
        }
    }
    Vec_push(&self->variables, &variable);
}

SResult VariableManager_get(inout VariableManager* self, in char* name, out Variable* variable) {
    for(u32 i=0; i<Vec_len(&self->variables); i++) {
        Variable* ptr = Vec_index(&self->variables, i);
        if(strcmp(ptr->name, name) == 0) {
            *variable = Variable_clone(ptr);
            return SResult_new(NULL);
        }
    }
    
    char msg[256];
    snprintf(msg, 256, "variable \"%.10s\" is undefiend", name);
    return SResult_new(msg);
}

void VariableManager_print(in VariableManager* self) {
    printf("VariableManager { variables: ");
    Vec_print(&self->variables, Variable_print_for_vec);
    printf(", stack_offset: %d }", self->stack_offset);
}

void VariableManager_print_for_vec(in void* ptr) {
    VariableManager_print(ptr);
}

VariableManager VariableManager_clone(in VariableManager* self) {
    VariableManager variable_manager = {Vec_clone(&self->variables, Variable_clone_for_vec), self->stack_offset};
    return variable_manager;
}

void VariableManager_free(VariableManager self) {
    Vec_free(self.variables);
}

void VariableManager_free_for_vec(inout void* ptr) {
    VariableManager* variable_manager = ptr;
    VariableManager_free(*variable_manager);
}

static bool resolve_parsermsg(ParserMsg msg, inout Generator* generator) {
    if(!ParserMsg_is_success(msg)) {
        Error error = Error_from_parsermsg(msg);
        Generator_add_error(generator, error);
        return true;
    }
    return false;
}

static bool resolve_sresult(SResult result, Offset offset, inout Generator* generator) {
    if(!SResult_is_success(result)) {
        Generator_add_error(generator, Error_from_sresult(offset, result));
        return true;
    }
    return false;
}

static void check_parser(in Parser* parser, inout Generator* generator) {
    if(!Parser_is_empty(parser)) {
        Generator_add_error(generator, Error_new(parser->offset, "unexpected token"));
    }
}

static bool GlobalSyntax_parse_struct_definision(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "struct"))) {
        return false;
    }
    
    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_StructDefinision;
    global_syntax->ok_flag = false;
    
    Type type;
    if(resolve_parsermsg(Type_parse_struct(&parser, generator, &type), generator)) {
        return true;
    }
    
    if(resolve_sresult(Generator_add_type(generator, type), parser.offset, generator)) {
        return true;
    }

    global_syntax->ok_flag = true;

    check_parser(&parser, generator);
    return true;
}

static bool GlobalSyntax_parse_enum_definision(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "enum"))) {
        return false;
    }
    
    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_EnumDefinision;
    global_syntax->ok_flag = false;
    
    Type type;
    if(resolve_parsermsg(Type_parse_enum(&parser, &type), generator)) {
        return true;
    }
    
    if(resolve_sresult(Generator_add_type(generator, type), parser.offset, generator)) {
        return true;
    }
    
    global_syntax->ok_flag = true;

    check_parser(&parser, generator);
    return true;
}

static bool GlobalSyntax_parse_asmacro_definision(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    if(!Parser_start_with(&parser, "as")) {
        return false;
    }

    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_AsmacroDefinision;
    global_syntax->ok_flag = false;

    Asmacro asmacro;
    if(resolve_parsermsg(Asmacro_parse(&parser, generator, &asmacro), generator)) {
        return true;
    }

    global_syntax->body.asmacro_definision = Asmacro_clone(&asmacro);
    global_syntax->ok_flag = true;

    if(resolve_sresult(Generator_add_asmacro(generator, asmacro), parser.offset, generator)) {
        return true;
    }

    check_parser(&parser, generator);
    return true;
}

static bool GlobalSyntax_parse_type_alias(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "type"))) {
        return false;
    }

    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_TypeAlias;
    global_syntax->ok_flag = false;

    char name[256];
    if(resolve_parsermsg(Parser_parse_ident(&parser, name), generator)) {
        return true;
    }

    Type type;
    if(resolve_parsermsg(Type_parse(&parser, generator, &type), generator)) {
        return true;
    }

    if(resolve_sresult(Generator_add_type(generator, type), parser.offset, generator)) {
        return true;
    }

    global_syntax->ok_flag = true;
    return true;
}

static ParserMsg function_definision_parse_arguments(Parser parser, in Generator* generator, inout VariableManager* variable_manager, inout Vec* arguments) {
    i32 stack_offset = 0;
    
    while(!Parser_is_empty(&parser)) {
        Parser parser_copy = parser;
        i32 dummy = 0;
        Variable variable;
        PARSERMSG_UNWRAP(
            Variable_parse(&parser_copy, generator, &dummy, &variable),
            (void)NULL
        );
        u32 size = variable.data.type.size;
        u32 align = variable.data.type.align;
        stack_offset -= size;
        stack_offset = (stack_offset - align + 1)/align*align;
        
        Variable_free(variable);
        
        PARSERMSG_UNWRAP(
            Variable_parse(&parser, generator, &stack_offset, &variable),
            (void)NULL
        );
        VariableManager_push(variable_manager, Variable_clone(&variable));
        Vec_push(arguments, &variable);
        
        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, ","),
                (void)NULL
            );
        }
    }
    
    return ParserMsg_new(parser.offset, NULL);
}

static bool GlobalSyntax_parse_function_definision(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "fn"))) {
        return false;
    }

    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_FunctionDefinision;
    global_syntax->ok_flag = false;

    char name[256];
    Parser paren_parser;
    VariableManager variable_manager = VariableManager_new(8);
    Vec arguments = Vec_new(sizeof(Variable));
    Parser block_parser;

    if(resolve_parsermsg(Parser_parse_ident(&parser, name), generator)
        || resolve_parsermsg(Parser_parse_paren(&parser, &paren_parser), generator)
        || resolve_parsermsg(function_definision_parse_arguments(paren_parser, generator, &variable_manager, &arguments), generator)
        || resolve_parsermsg(Parser_parse_block(&parser, &block_parser), generator)) {
            
        VariableManager_free(variable_manager);
        Vec_free_all(arguments, Variable_free_for_vec);
        return true;
    }

    Asmacro wrapper_asmacro = Asmacro_new_fn_wrapper(name, arguments);
    if(resolve_sresult(Generator_add_asmacro(generator, wrapper_asmacro), parser.offset, generator)) {
        return true;
    }

    check_parser(&parser, generator);
    
    strcpy(global_syntax->body.function_definision.name, name);
    global_syntax->body.function_definision.variable_manager = variable_manager;
    global_syntax->body.function_definision.proc_parser = block_parser;

    global_syntax->ok_flag = true;
    return true;
}

ParserMsg GlobalSyntax_parse(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    bool (*BUILDERS[])(Parser, inout Generator*, out GlobalSyntax*) = {
        GlobalSyntax_parse_struct_definision,
        GlobalSyntax_parse_enum_definision,
        GlobalSyntax_parse_asmacro_definision,
        GlobalSyntax_parse_type_alias,
        GlobalSyntax_parse_function_definision,
    };
    
    for(u32 i=0; i<LEN(BUILDERS); i++) {
        if(BUILDERS[i](parser, generator, global_syntax)) {
            return ParserMsg_new(parser.offset, NULL);
        }
    }
    
    return ParserMsg_new(parser.offset, "unknown global syntax");
}

void GlobalSyntax_check_asmacro(inout GlobalSyntax* self, inout Generator* generator) {
    (void)generator;
    if(self->type != GlobalSyntax_AsmacroDefinision) {
        return;
    }

    if(self->body.asmacro_definision.type == Asmacro_UserOperator) {
        TODO();
    }
}

static void GlobalSyntax_build_function_definision(inout GlobalSyntax* self, inout Generator* generator) {
    Parser parser = self->body.function_definision.proc_parser;
    VariableManager* variable_manager = &self->body.function_definision.variable_manager;
    
    Label label;
    strcpy(label.name, self->body.function_definision.name);
    label.public_flag = true;
    strcpy(label.section_name, "text");
    resolve_sresult(
        Generator_binary_len(generator, "text", &label.offset),
        parser.offset,
        generator
    );
    resolve_sresult(
        Generator_new_label(generator, label),
        parser.offset,
        generator
    );

    while(!Parser_is_empty(&parser)) {
        Parser syntax_parser = Parser_split(&parser, ";");
        Data data;
        
        resolve_sresult(
            Syntax_build(syntax_parser, generator, variable_manager, &data), parser.offset, generator
        );

        Data_free(data);
    }
}

void GlobalSyntax_build(inout GlobalSyntax* self, inout Generator* generator) {
    (void)self;(void)generator;
    switch(self->type) {
        case GlobalSyntax_StructDefinision:
        case GlobalSyntax_EnumDefinision:
        case GlobalSyntax_TypeAlias:
        case GlobalSyntax_AsmacroDefinision:
            break;
        case GlobalSyntax_FunctionDefinision:
            GlobalSyntax_build_function_definision(self, generator);
            break;
    }
}

void GlobalSyntax_print(in GlobalSyntax* self) {
    printf("GlobalSyntax { offset: ");
    Offset_print(&self->offset);
    printf("ok_flag: %s, type: %d, body: ",
        BOOL_TO_STR(self->ok_flag),
        self->type
    );

    switch(self->type) {
        case GlobalSyntax_StructDefinision:
            printf(".none");
            break;
        case GlobalSyntax_EnumDefinision:
            printf(".none");
            break;
        case GlobalSyntax_AsmacroDefinision:
            printf(".asmacro_definision: ");
            Asmacro_print(&self->body.asmacro_definision);
            break;
        case GlobalSyntax_TypeAlias:
            printf(".none");
            break;
        case GlobalSyntax_FunctionDefinision:
            printf(".function_definision: name: %s, variable_manager: ", self->body.function_definision.name);
            VariableManager_print(&self->body.function_definision.variable_manager);
            break;
    }
    printf(" }");
}

void GlobalSyntax_print_for_vec(in void* ptr) {
    GlobalSyntax_print(ptr);
}

void GlobalSyntax_free(GlobalSyntax self) {
    switch(self.type) {
        case GlobalSyntax_StructDefinision:
        case GlobalSyntax_EnumDefinision:
        case GlobalSyntax_TypeAlias:
            break;
        case GlobalSyntax_AsmacroDefinision:
            Asmacro_free(self.body.asmacro_definision);
            break;
        case GlobalSyntax_FunctionDefinision:
            VariableManager_free(self.body.function_definision.variable_manager);
            break;
    }
}

void GlobalSyntax_free_for_vec(inout void* ptr) {
    GlobalSyntax* global_syntax = ptr;
    GlobalSyntax_free(*global_syntax);
}

GlobalSyntaxTree GlobalSyntaxTree_new() {
    GlobalSyntaxTree tree = {Generator_new(), Vec_new(sizeof(GlobalSyntax))};
    Generator_new_section(&tree.generator, "text");
    return tree;
}

void GlobalSyntaxTree_parse(inout GlobalSyntaxTree* self, Parser parser) {
    while(!Parser_is_empty(&parser)) {
        Parser syntax_parser = Parser_split(&parser, ";");
        
        GlobalSyntax global_syntax;
        if(!resolve_parsermsg(GlobalSyntax_parse(syntax_parser, &self->generator, &global_syntax), &self->generator)) {
            Vec_push(&self->global_syntaxes, &global_syntax);
        }
    }
}

void GlobalSyntaxTree_check_asmacro(inout GlobalSyntaxTree* self) {
    for(u32 i=0; i<Vec_len(&self->global_syntaxes); i++) {
        GlobalSyntax* global_syntax = Vec_index(&self->global_syntaxes, i);
        GlobalSyntax_check_asmacro(global_syntax, &self->generator);
    }
}

Generator GlobalSyntaxTree_build(inout GlobalSyntaxTree self) {
    for(u32 i=0; i<Vec_len(&self.global_syntaxes); i++) {
        GlobalSyntax* global_syntax = Vec_index(&self.global_syntaxes, i);
        GlobalSyntax_build(global_syntax, &self.generator);
    }
    
    Vec_free_all(self.global_syntaxes, GlobalSyntax_free_for_vec);
    return self.generator;
}

void GlobalSyntaxTree_print(in GlobalSyntaxTree* self) {
    printf("GlobalSyntaxTree { generator: ");
    Generator_print(&self->generator);
    printf(", global_syntaxes: ");
    Vec_print(&self->global_syntaxes, GlobalSyntax_print_for_vec);
    printf(" }");
}

void GlobalSyntaxTree_free(GlobalSyntaxTree self) {
    Generator_free(self.generator);
    Vec_free_all(self.global_syntaxes, GlobalSyntax_free_for_vec);
}

SResult Syntax_build(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    bool (*BUILDERS[])(Parser, inout Generator*, inout VariableManager* variable_manager, out Data*) = {
        Syntax_build_asmacro_expansion,
        Syntax_build_register_expression,
        Syntax_build_imm_expression,
        Syntax_build_variable_expression,
    };
    
    for(u32 i=0; i<LEN(BUILDERS); i++) {
        Parser parser_copy = parser;
        if(BUILDERS[i](parser_copy, generator, variable_manager, data)) {
            return SResult_new(NULL);
        }
    }

    return SResult_new("unknown expression");
}

static SResult Syntax_build_asmacro_expansion_get_arguments(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Vec* arguments) {
    *arguments = Vec_new(sizeof(Data));

    while(!Parser_is_empty(&parser)) {
        Parser arg_parser = Parser_split(&parser, ",");
        
        Data arg;
        SRESULT_UNWRAP(
            Syntax_build(arg_parser, generator, variable_manager, &arg),
            Vec_free_all(*arguments, Data_free_for_vec)
        );

        Vec_push(arguments, &arg);
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_search_asmacro(in Vec* asmacroes, in Vec* arguments, out Asmacro* asmacro) {
    for(u32 i=0; i<Vec_len(asmacroes); i++) {
        Asmacro* ptr = Vec_index(asmacroes, i);
        if(SResult_is_success(Asmacro_match_with(ptr, arguments))) {
            *asmacro = Asmacro_clone(ptr);
            return SResult_new(NULL);
        }
    }

    return SResult_new("mismatching asmacro");
}

static bool Syntax_build_asmacro_expansion_get_info(
    in char* name,
    Parser args_parser,
    inout Generator* generator,
    inout VariableManager* variable_manager,
    out Asmacro* asmacro,
    out Vec* arguments
) {
    Vec asmacroes;
    if(resolve_sresult(Generator_get_asmacro(generator, name, &asmacroes), args_parser.offset, generator)) {
        return true;
    }

    if(resolve_sresult(Syntax_build_asmacro_expansion_get_arguments(args_parser, generator, variable_manager, arguments), args_parser.offset, generator)) {
        Vec_free_all(asmacroes, Asmacro_free_for_vec);
        return true;
    }

    if(resolve_sresult(Syntax_build_asmacro_expansion_search_asmacro(&asmacroes, arguments, asmacro), args_parser.offset, generator)) {
        Vec_free_all(asmacroes, Asmacro_free_for_vec);
        Vec_free_all(*arguments, Data_free_for_vec);
        return true;
    }

    Vec_free_all(asmacroes, Asmacro_free_for_vec);

    return false;
}

static SResult Syntax_build_asmacro_expansion_asmoperator(in Asmacro* asmacro, in Vec* arguments, inout Generator* generator) {
    AsmArgs asm_args;

    SRESULT_UNWRAP(
        AsmArgs_from(arguments, &asmacro->arguments, &asm_args),
        (void)NULL
    );
    SRESULT_UNWRAP(
        AsmEncoding_encode(&asmacro->body.asm_operator, &asm_args, generator),
        (void)NULL
    );
    
    return SResult_new(NULL);
}

static void Syntax_build_asmacro_expansion_useroperator(in Asmacro* asmacro, in Vec* dataes, inout Generator* generator, inout VariableManager* variable_manager) {
    Parser proc_parser = asmacro->body.user_operator.parser;
    
    for(u32 i=0; i<Vec_len(dataes); i++) {
        Argument* arg = Vec_index(&asmacro->arguments, i);
        Data* data = Vec_index(dataes, i);

        Variable variable;
        strcpy(variable.name, arg->name);
        variable.data = Data_clone(data);

        VariableManager_push(variable_manager, variable);
    }

    while(!Parser_is_empty(&proc_parser)) {
        Parser syntax_parser = Parser_split(&proc_parser, ";");
        
        Data data;
        resolve_sresult(
            Syntax_build(syntax_parser, generator, variable_manager, &data),
            syntax_parser.offset, generator
        );
        Data_free(data);
    }
}

bool Syntax_build_asmacro_expansion(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    char name[256];
    if(!ParserMsg_is_success(Parser_parse_ident(&parser, name))) {
        return false;
    }
    Parser args_parser;
    if(!ParserMsg_is_success(Parser_parse_paren(&parser, &args_parser))) {
        return false;
    }

    Asmacro asmacro;
    Vec arguments;
    if(Syntax_build_asmacro_expansion_get_info(
        name,
        args_parser,
        generator,
        variable_manager,
        &asmacro,
        &arguments)) {
        return true;
    }

    if(Vec_len(&arguments) != 0) {
        *data = Data_clone(Vec_index(&arguments, Vec_len(&arguments)-1));
    }else {
        *data = DATA_VOID;
    }

    switch(asmacro.type) {
        case Asmacro_AsmOperator:
            resolve_sresult(
                Syntax_build_asmacro_expansion_asmoperator(&asmacro, &arguments, generator),
                parser.offset,
                generator
            );
            break;
        case Asmacro_UserOperator:
            Syntax_build_asmacro_expansion_useroperator(&asmacro, &arguments, generator, variable_manager);
            break;
        case Asmacro_FnWrapper:
            TODO();
            break;
    }

    Asmacro_free(asmacro);
    Vec_free_all(arguments, Data_free_for_vec);

    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_variable_declaration(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "let"))) {
        return false;
    }
    
    *data = DATA_VOID;
    
    Variable variable;
    if(resolve_parsermsg(Variable_parse(&parser, generator, &variable_manager->stack_offset, &variable), generator)) {
        return true;
    }
    VariableManager_push(variable_manager, variable);
    
    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_register_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    (void)generator;(void)variable_manager;
    Register reg;
    
    if(!ParserMsg_is_success(Register_parse(&parser, &reg))) {
        return false;
    }
    
    *data = Data_from_register(reg);
    
    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_imm_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    (void)variable_manager;
    u64 imm;
    if(!ParserMsg_is_success(Parser_parse_number(&parser, &imm))) {
        return false;
    }
    
    *data = Data_from_imm(imm);
    
    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_variable_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    (void)generator;
    
    char name[256];
    if(!ParserMsg_is_success(Parser_parse_ident(&parser, name))) {
        return false;
    }
    
    *data = DATA_VOID;
    
    Variable variable;
    if(resolve_sresult(VariableManager_get(variable_manager, name, &variable), parser.offset, generator)) {
        return true;
    }
    
    *data = variable.data;
    
    check_parser(&parser, generator);
    return true;
}





