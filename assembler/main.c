#include <stdio.h>
#include "types.h"
#include "cmd.h"
#include "tokenizer.h"

int main(int argc, char* argv[]) {
    // return Cmd_interpreter(argc, argv);
    
    char src[] = "printf(\"Hello!\")";
    TokenField field = TokenField_new(src, "main.amc");
    TokenField_print(&field);
    TokenField_free(field);
    printf("\n");

    return 0;
}

