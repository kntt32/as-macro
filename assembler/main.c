#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "gen.h"
#include "syntax.h"

void test2() {
    Parser parser = Parser_new("\
    as add(x: i32@reg+mem, y: i32@imm) : (double: 0x81, /0, id);\n\
    fn ADD(x: i32@rax) {\n\
        add(x, 10);\n\
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

int main() {
    test2();

    return 0;
}

