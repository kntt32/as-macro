#include <stdio.h>
#include <stdbool.h>
#include "types.h"
#include "gen.h"
#include "syntax.h"

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

static bool GlobalSyntax_build_struct(Parser parser, inout Generator* generator) {
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

static bool GlobalSyntax_build_enum(Parser parser, inout Generator* generator) {
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

static bool GlobalSyntax_build_define_asmacro(Parser parser, inout Generator* generator) {
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

static ParserMsg GlobalSyntax_build_type_parse(Parser parser, inout Generator* generator, out Type* type) {
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

static ParserMsg GlobalSyntax_build_function_parse_arguments(Parser parser, inout Generator* generator, inout Vec* args) {
    // $ident: $type @ (register/stack), ..
    i32 rbp_offset = -8;
    while(!Parser_is_empty(&parser)) {
        Variable variable;
        PARSERMSG_UNWRAP(
            Variable_parse(&parser, generator, rbp_offset, &variable),
            (void)NULL
        );

        Storage* storage = Variable_get_storage(&variable);
        switch(storage->type) {
            case StorageType_reg:
                break;
            case StorageType_mem:
            {
                Type* type = Variable_get_type(&variable);
                rbp_offset -= type->size;
                rbp_offset = (rbp_offset - type->align + 1)/type->align*type->align;
                storage->body.mem.disp.body.offset = rbp_offset;
            }
                break;
            case StorageType_xmm:
                break;
            case StorageType_imm:
                break;
        }

        Vec_push(args, &variable);
        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, ","),
                (void)NULL
            );
        }
    }

    return SUCCESS_PARSER_MSG;
}

static ParserMsg GlobalSyntax_build_function_parse(
    Parser parser, inout Generator* generator, out char name[256], inout Vec* args, out Parser* proc_parser) {
    PARSERMSG_UNWRAP(
        Parser_parse_keyword(&parser, "fn"),
        (void)NULL
    );
    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser, name),
        (void)NULL
    );

    Parser paren_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser, &paren_parser),
        (void)NULL
    );
    PARSERMSG_UNWRAP(
        GlobalSyntax_build_function_parse_arguments(paren_parser, generator, args),
        (void)NULL
    );

    PARSERMSG_UNWRAP(
        Parser_parse_block(&parser, proc_parser),
        (void)NULL
    );

    return SUCCESS_PARSER_MSG;
}

static bool GlobalSyntax_build_function(Parser parser, inout Generator* generator) {
    if(!Parser_start_with(&parser, "fn")) {
        return false;
    }

    char name[256];
    Vec vec = Vec_new(sizeof(Variable));
    Parser proc_parser;
    if(GlobalSyntax_resolve_parsermsg(
        GlobalSyntax_build_function_parse(parser, generator, name, &vec, &proc_parser), generator)) {
        Vec_free_all(vec, Variable_free_for_vec);
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

