#include <stdio.h>
#include "types.h"
#include "cmd.h"

int main(int argc, char* argv[]) {
    return Cmd_interpreter(argc, argv);
}

