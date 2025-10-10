#include <stdio.h>
#include "types.h"
// #include "cmd.h"
#include "tokenizer.h"
#include "fixed_parser.h"

int main(int argc, char* argv[]) {
    // return Cmd_interpreter(argc, argv);
    
    char src[] = "\"Hello\"; printf (123);";
    TokenField field = TokenField_new(src, "main.amc");
    TokenField_print(&field);
    printf("\n");

    FixedParser parser = FixedParser_new(&field);
    char ident[256] = "";
    FixedParser parser2 = FixedParser_split(&parser, ";");
    printf("%lu, %lu\n", parser2.len, parser2.cursor);
    printf("%lu, %lu\n", parser.len, parser.cursor);

    FixedParserMsg msg = FixedParser_parse_keyword(&parser, "printf");
    msg = FixedParser_parse_symbol(&parser, "(");
    u64 value;
    msg = FixedParser_parse_number(&parser, &value);
    if(FixedParserMsg_is_success(msg)) {
        printf("success: %lu", value);
    }else {
        printf("msg: %s\n", msg.msg);
    }

    TokenField_free(field);
    printf("\n");

    return 0;
}

