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
\n\
    as area(x: u32@reg.4, y: u32@reg.4) {\n\
        imul x, y\n\
    };\n\
    as mul(x: u32@reg.4, y: u32@reg.4) {\n\
        imul x, y\n\
    };\n\
    ");
    Generator gen = GlobalSyntax_build(parser);
    Generator_print(&gen);
    printf("\n");
    Generator_free(gen);
    return 0;
}

