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
            return SRESULT_OK;
        }
    }
    
    SResult result;
    result.ok_flag = false;
    snprintf(result.error, 256, "variable \"%.10s\" is undefiend", name);
    return result;
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

static void GlobalSyntax_check_parser(in Parser* parser, inout Generator* generator) {
    if(!Parser_is_empty(parser)) {
        Error error = {parser->line, "unexpected token"};
        Generator_add_error(generator, error);
    }
}

static bool GlobalSyntax_resolve_parsermsg(ParserMsg msg, inout Generator* generator) {
    if(!ParserMsg_is_success(msg)) {
        Error error = Error_from_parsermsg(msg);
        Generator_add_error(generator, error);
        return true;
    }
    return false;
}

bool GlobalSyntax_build_struct(Parser parser, inout Generator* generator) {
    // struct $name { .. }
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "struct"))) {
        return false;
    }

    Type type;
    ParserMsg msg = Type_parse_struct(&parser, generator, &type);
    if(!ParserMsg_is_success(msg)) {
        Generator_add_error(generator, Error_from_parsermsg(msg));
        return true;
    }

    SResult result = Generator_add_type(generator, type);
    if(!SRESULT_IS_OK(result)) {
        Generator_add_error(generator, Error_from_sresult(parser.line, result));
        return true;
    }

    GlobalSyntax_check_parser(&parser, generator);

    return true;
}

bool GlobalSyntax_build_enum(Parser parser, inout Generator* generator) {
    // enum $name { .. }
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "enum"))) {
        return false;
    }

    Type type;
    ParserMsg msg = Type_parse_enum(&parser, &type);
    if(!ParserMsg_is_success(msg)) {
        Generator_add_error(generator, Error_from_parsermsg(msg));
        return true;
    }

    SResult result = Generator_add_type(generator, type);
    if(!SRESULT_IS_OK(result)) {
        Generator_add_error(generator, Error_from_sresult(parser.line, result));
        return true;
    }

    GlobalSyntax_check_parser(&parser, generator);

    return true;
}

bool GlobalSyntax_build_define_asmacro(Parser parser, inout Generator* generator) {
    // as $name ( .. ) { .. }
    // as $name ( .. ) : ( .. )
    if(!Parser_start_with(&parser, "as")) {
        return false;
    }

    Asmacro asmacro;
    ParserMsg msg = Asmacro_parse(&parser, generator, &asmacro);
    if(!ParserMsg_is_success(msg)) {
        Generator_add_error(generator, Error_from_parsermsg(msg));
        return true;
    }
    Generator_add_asmacro(generator, asmacro);

    GlobalSyntax_check_parser(&parser, generator);

    return true;
}

ParserMsg GlobalSyntax_build_type_parse(Parser parser, inout Generator* generator, out Type* type) {
    char alias[256];
    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser, alias), (void)NULL
    );

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser, "="), (void)NULL
    );

    PARSERMSG_UNWRAP(
        Type_parse(&parser, generator, type), (void)NULL
    );

    strcpy(type->name, alias);

    return SUCCESS_PARSER_MSG;
}

static bool GlobalSyntax_build_type(Parser parser, inout Generator* generator) {
    // type $alias = $type;
    if(GlobalSyntax_resolve_parsermsg(
        Parser_parse_keyword(&parser, "type"), generator
    )) {
        return false;
    }

    Type type;
    if(GlobalSyntax_resolve_parsermsg(GlobalSyntax_build_type_parse(parser, generator, &type), generator)) {
        return true;
    }

    SResult result = Generator_add_type(generator, type);
    if(!SRESULT_IS_OK(result)) {
        Error error = Error_from_sresult(parser.line, result);
        Generator_add_error(generator, error);
        return true;
    }

    return true;
}

typedef struct {
    char name[256];
    Vec arguments;// Vec<Variable>
    Parser proc_parser;
} Function;

static ParserMsg GlobalSyntax_build_function_definision_parse_arguments(Parser parser, inout Generator* generator, inout Vec* arguments) {
    while(!Parser_is_empty(&parser)) {
        Variable variable;
        PARSERMSG_UNWRAP(
            Variable_parse(&parser, generator, 16, &variable),
            (void)NULL
        );

        Vec_push(arguments, &variable);
        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, ","),
                (void)NULL
            );
        }
    }

    return SUCCESS_PARSER_MSG;
}

static ParserMsg GlobalSyntax_build_function_definision_parse(Parser parser, inout Generator* generator, out Function* function) {
    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser, function->name),
        (void)NULL
    );
    Parser paren_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser, &paren_parser),
        (void)NULL
    );
    PARSERMSG_UNWRAP(
        Parser_parse_block(&parser, &function->proc_parser),
        (void)NULL
    );

    function->arguments = Vec_new(sizeof(Variable));
    PARSERMSG_UNWRAP(
        GlobalSyntax_build_function_definision_parse_arguments(paren_parser, generator, &function->arguments),
        Vec_free_all(function->arguments, Variable_free_for_vec)
    );

    return SUCCESS_PARSER_MSG;
}

bool GlobalSyntax_build_function_definision(Parser parser, inout Generator* generator) {
    // fn $name ( .. ) { .. }
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "fn"))) {
        return false;
    }

    Function function;
    if(GlobalSyntax_resolve_parsermsg(GlobalSyntax_build_function_definision_parse(parser, generator, &function), generator)) {
        return true;
    }
    TODO();

    return true;
}

Generator GlobalSyntax_build(Parser parser) {
    static bool (*BUILDERS[])(Parser, inout Generator*) = {
        GlobalSyntax_build_struct,
        GlobalSyntax_build_enum,
        GlobalSyntax_build_define_asmacro,
        GlobalSyntax_build_function_definision,
        GlobalSyntax_build_type,
    };
    Generator generator = Generator_new();

    bool matched_flag;
    while(!Parser_is_empty(&parser)) {
        matched_flag = false;
        Parser syntax_parser;
        Parser_split(&parser, ";", &syntax_parser);

        for(u32 i=0; i<LEN(BUILDERS); i++) {
            if(BUILDERS[i](syntax_parser, &generator)) {
                matched_flag = true;
                break;
            }
        }
        if(!matched_flag) {
            Error error =  {parser.line, "unknown syntax"};
            Generator_add_error(&generator, error);
        }
    }

    return generator;
}




