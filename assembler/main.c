#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "gen.h"
#include "syntax.h"

int main() {
    printf("hello, world!\n");
    Parser parser_ = Parser_new("\
    struct Point {\
        x: u32,\
        y: u32,\
    }");
    Generator gen = GlobalSyntax_build(parser_);
    Parser parser = Parser_new("a: struct { x: [u32; 5]} @stack");
    Variable variable;
    ParserMsg msg = Variable_parse(&parser, &gen, 0, &variable);
    if(ParserMsg_is_success(msg)) {
        Variable_print(&variable);
        printf("\n");
        Generator_print(&gen);
        printf("\n");
        Variable_free(variable);
    }else {
        printf("%s\n", msg.msg);
    }
    Generator_free(gen);
    return 0;
}

