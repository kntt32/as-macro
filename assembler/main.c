#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "gen.h"

int main() {
    printf("hello, world!\n");
    Parser parser = Parser_new("a: enum { A = 3, B, C = 5, D }* @rax");
    Generator gen = Generator_new();
    Variable variable;
    ParserMsg msg = Variable_parse(&parser, &gen, 0, &variable);
    if(ParserMsg_is_success(msg)) {
        Variable_print(&variable);
        printf("\n");
        Variable_free(variable);
    }else {
        printf("%s\n", msg.msg);
    }
    Generator_free(gen);
    return 0;
}

