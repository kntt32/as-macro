#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "gen.h"
#include "syntax.h"

void test2() {
    Parser parser = Parser_new("\
    as add(x: i32@reg+mem, y: i32@reg) : (double: 0x01, /r);\n\
    fn ADD(x: i32@rax, y: i32@rdi) {\n\
        add(x, y);\n\
    };\n\
    ");
    GlobalSyntaxTree tree = GlobalSyntaxTree_new();
    GlobalSyntaxTree_parse(&tree, parser);
    GlobalSyntaxTree_check_asmacro(&tree);
    GlobalSyntaxTree_print(&tree);
    printf("\n");
    GlobalSyntaxTree_free(tree);
}

int main() {
    test2();

    return 0;
}

