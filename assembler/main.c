#include <stdio.h>
#include "types.h"
// #include "cmd.h"
#include "tokenizer.h"
#include "fixed_parser.h"

int main(int argc, char* argv[]) {
    // return Cmd_interpreter(argc, argv);
    
    char src[] = "printf (123);";
    TokenField field = TokenField_new(src, "main.amc");
    TokenField_print(&field);
    printf("\n");

    FixedParser parser = FixedParser_new(&field);
    FixedParserMsg msg = FixedParser_parse_keyword(&parser, "printf");
    FixedParser parser2;
    msg = FixedParser_parse_paren(&parser, &parser2);
    if(FixedParserMsg_is_success(msg)) {
        printf("success\n");
        FixedParser_print(&parser);
        printf("\n");
        FixedParser_print(&parser2);
    }else {
        printf("msg: %s\n", msg.msg);
    }

    TokenField_free(field);
    printf("\n");

    return 0;
}

