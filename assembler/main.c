#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "gen.h"
#include "syntax.h"
#include "cmd.h"

int main(int argc, char* argv[]) {
    Cmd_interpreter(argc, argv);

    return 0;
}

