#include <stdio.h>
#include <stdbool.h>
#include "types.h"
#include "gen.h"
#include "syntax.h"

static bool GlobalSyntax_build_struct(inout Parser* parser, inout Generator* generator) {
    if(!ParserMsg_is_success(Parser_parse_keyword(parser, "struct"))) {
        return false;
    }

    Type type;
    ParserMsg msg = Type_parse_struct(parser, generator, &type);
    if(!ParserMsg_is_success(msg)) {
        Generator_add_error(generator, Error_from_parsermsg(msg));
        return false;
    }

    SResult result = Generator_add_type(generator, type);
    if(!SRESULT_IS_OK(result)) {
        Generator_add_error(generator, Error_from_sresult(parser->line, result));
        return false;
    }

    return true;
}

bool GlobalSyntax_build_define_asmacro(inout Parser* parser, inout Generator* generator) {
    if(!Parser_start_with(parser, "as")) {
        return false;
    }

    Asmacro asmacro;
    ParserMsg msg = Asmacro_parse(parser, generator, &asmacro);
    TODO();
    return true;
}

Generator GlobalSyntax_build(Parser parser) {
    static bool (*BUILDERS[])(inout Parser*, inout Generator*) = {GlobalSyntax_build_struct};
    Generator generator = Generator_new();

    bool matched_flag;
    while(!Parser_is_empty(&parser)) {
        matched_flag = false;
        for(u32 i=0; i<LEN(BUILDERS); i++) {
            if(BUILDERS[i](&parser, &generator)) {
                matched_flag = true;
                break;
            }
        }
        if(!matched_flag) {
            Error error =  {parser.line, "unknown syntax"};
            Generator_add_error(&generator, error);
            break;
        }
    }

    return generator;
}

