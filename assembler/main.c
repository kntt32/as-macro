#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "gen.h"

int main() {
    printf("hello, world!");
    Parser parser = Parser_new("u32 **");
    Generator gen = Generator_new();
    Type type;
    ParserMsg msg = Type_parse(&parser, &gen, &type);
    if(ParserMsg_is_success(msg)) {
        Type_print(&type);
        printf("\n");
        Type_free(type);
    }else {
        printf("%s", msg.msg);
    }
    Generator_free(gen);
    return 0;
}

