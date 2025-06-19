#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "gen.h"
#include "syntax.h"

int main() {
    printf("hello, world!\n");


    Parser parser = Parser_new("\n\
    struct Point {\n\
        x: u32,\n\
        y: u32,\n\
    };\n\
    enum MyEnum {\n\
        MyEnum_A,\n\
        MyEnum_B = 3,\n\
        MyEnum_C,\n\
    };\n\
    type ptr = Point;\n\
\n\
    \n\
    as area(x: u32@reg, y: u32@reg) {\n\
        imul x, y\n\
    };\n\
    as mul(x: u32@reg, y: u32@reg) {\n\
        imul x, y\n\
    };\n\
    as imul(x: u32@reg, y:u32@reg/mem): (double: 0x0f, 0xaf, /r);\n\
    ");
    Generator gen = GlobalSyntax_build(parser);
    Generator_print(&gen);
    printf("\n");
    Generator_free(gen);
/*

    Parser parser = Parser_new("as imul(x: u32@reg, y:u32@reg/mem): (double: 0x0f, 0xaf, /r)");
    Generator generator = Generator_new();
    Asmacro asmacro;
    if(ParserMsg_is_success(Asmacro_parse(&parser, &generator, &asmacro))) {
        Asmacro_print(&asmacro);
        Asmacro_free(asmacro);
    }else {
printf("failed");
    }
    Generator_free(generator);
    printf("\n");*/
    return 0;
}

