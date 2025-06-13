#include <stdio.h>
#include "types.h"
#include "register.h"

static struct { Register reg; char* str; } REGISTER_TABLE[] = {
    {Rsp, "rsp"},
    {Rbp, "rbp"},

    {Rax, "rax"},
    {Rcx, "rcx"},
    {Rdx, "rdx"},
    {Rbx, "rbx"},
    {Rsi, "rsi"},
    {Rdi, "rdi"},
    {R8, "r8"},
    {R9, "r9"},
    {R10, "r10"},
    {R11, "r11"},
    {R12, "r12"},
    {R13, "r13"},
    {R14, "r14"},
    {R15, "r15"},

    {Xmm0, "xmm0"},
    {Xmm1, "xmm1"},
    {Xmm2, "xmm2"},
    {Xmm3, "xmm3"},
    {Xmm4, "xmm4"},
    {Xmm5, "xmm5"},
    {Xmm6, "xmm6"},
    {Xmm7, "xmm7"},
    {Xmm8, "xmm8"},
    {Xmm9, "xmm9"},
    {Xmm10, "xmm10"},
    {Xmm11, "xmm11"},
    {Xmm12, "xmm12"},
    {Xmm13, "xmm13"},
    {Xmm14, "xmm14"},
    {Xmm15, "xmm15"},
};

ParserMsg Register_parse(Parser* parser, Register* restrict ptr) {
    for(u32 i=0; i<LEN(REGISTER_TABLE); i++) {
        char* rtable_str = REGISTER_TABLE[i].str;
        Register rtable_reg = REGISTER_TABLE[i].reg;

        if(Parser_parse_keyword(parser, rtable_str).msg[0] == '\0') {
            *ptr = rtable_reg;
            return SUCCESS_PARSER_MSG;
        }
    }

    ParserMsg msg = {parser->line, "expected keyword"};
    return msg;
}

void Register_print(in Register self) {
    for(u32 i=0; i<LEN(REGISTER_TABLE); i++) {
        char* rtable_str = REGISTER_TABLE[i].str;
        Register rtable_reg = REGISTER_TABLE[i].reg;

        if(self == rtable_reg) {
            printf("%s", rtable_str);
            return;
        }
    }

    PANIC("unknown value");
    
    return;
}

