#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "gen.h"
#include "syntax.h"

/*
void test() {
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
    as mov(x: u32@reg, y: u32@imm): (double: 0xb8, rd, id);\n\
    ");
    Generator gen = GlobalSyntax_build(parser);
    Generator_print(&gen);
    printf("\n");
    Generator_free(gen);
}
*/
void test2() {
    Parser parser = Parser_new("\n\
    fn add(x: u32@rax, y: u32@rdi) {\n\
        add(x, y);\n\
    }\n\
    ");
    Generator gen = Generator_new();
    GlobalSyntax global_syntax;
    ParserMsg msg = GlobalSyntax_parse(parser, &gen, &global_syntax);
    if(ParserMsg_is_success(msg)) {
        Generator_print(&gen);
        printf("\n\n");
        GlobalSyntax_print(&global_syntax);
        GlobalSyntax_free(global_syntax);
    }else {
        printf("failed");
    }
    printf("\n");
    Generator_free(gen);
}

int main() {
    test2();

    return 0;
}

