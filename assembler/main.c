#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "gen.h"
#include "syntax.h"
#include "cmd.h"

void test2() {
    Parser parser = Parser_new("\
    as mov(x: i64@reg, y: i64@imm) : (double: 0xb8, rq, iq);\n\
    as nop() : (double: 0x90);\n\
    as NOP() {\n\
        nop();\n\
    };\n\
    fn f() {\n\
        NOP();\n\
    };\n\
    fn main(x: i64@r15) {\n\
        mov(x, 10);\n\
        nop();\n\
    }\n\
    ");
    GlobalSyntaxTree tree = GlobalSyntaxTree_new();
    GlobalSyntaxTree_parse(&tree, parser);
    // GlobalSyntaxTree_check_asmacro(&tree);
    // GlobalSyntaxTree_print(&tree);
    Generator generator = GlobalSyntaxTree_build(tree);
    Generator_print(&generator);
    Generator_free(generator);
    // GlobalSyntaxTree_free(tree);
    printf("\n");
}

int main(int argc, char* argv[]) {
    // test2();
    Cmd_interpreter(argc, argv);

    return 0;
}

