#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "gen.h"
#include "syntax.h"
#include "cmd.h"

int main(int argc, char* argv[]) {
    Parser parser = Parser_new("ab.cde.fgh.", "tset.amc");
    Parser left = Parser_rsplit(&parser, ".");
    Parser_print(&left);
    Parser_print(&parser);

    Cmd_interpreter(argc, argv);

    return 0;
}

