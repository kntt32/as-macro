#include <stdio.h>
#include "types.h"
// #include "cmd.h"
#include "tokenizer.h"
#include "fixed_parser.h"

int main(int argc, char* argv[]) {
    // return Cmd_interpreter(argc, argv);
    
    char src[] = "printf(\"Hello!)";
    TokenField field = TokenField_new(src, "main.amc");
    TokenField_print(&field);
    printf("\n");

    FixedParser parser = FixedParser_new(&field);
    char ident[256] = "";
    
    FixedParserMsg msg = FixedParser_parse_keyword(&parser, "printf");
    if(FixedParserMsg_is_success(msg)) {
        printf("success");
    }else {
        printf("msg: %s\n", msg.msg);
    }

    TokenField_free(field);
    printf("\n");

    return 0;
}

