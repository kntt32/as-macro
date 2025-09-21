#include <stdio.h>
#include "types.h"
#include "cmd.h"
#include "tokenizer.h"

int main(int argc, char* argv[]) {
    // return Cmd_interpreter(argc, argv);
    
    char src[] = "fn main() { let a: u32@rax = 5; let c: char@rdi = 'A'; };";
    TokenField field = TokenField_new(src, "main.amc");
    TokenField_print(&field);
    TokenField_free(field);

    return 0;
}

