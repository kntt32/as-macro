#include <stdio.h>
#include "types.h"
#include "register.h"

static struct { Register reg; char* str; u8 addreg_code} REGISTER_TABLE[] = {
    {Rsp, "rsp", 0x80 + 4},
    {Rbp, "rbp", 0x80 + 5},

    {Rax, "rax", 0},
    {Rcx, "rcx", 1},
    {Rdx, "rdx", 2},
    {Rbx, "rbx", 3},
    {Rsi, "rsi", 0x80 + 6},
    {Rdi, "rdi", 0x80 + 7},
    {R8, "r8", 8},
    {R9, "r9", 9},
    {R10, "r10", 10},
    {R11, "r11", 11},
    {R12, "r12", 12},
    {R13, "r13", 13},
    {R14, "r14", 14},
    {R15, "r15", 15},
};

static struct {Register reg; char* str;} XMM_TABLE[] = {
    {Xmm0, "xmm0", 0},
    {Xmm1, "xmm1", 0},
    {Xmm2, "xmm2", 0},
    {Xmm3, "xmm3", 0},
    {Xmm4, "xmm4", 0},
    {Xmm5, "xmm5", 0},
    {Xmm6, "xmm6", 0},
    {Xmm7, "xmm7", 0},
    {Xmm8, "xmm8", 0},
    {Xmm9, "xmm9", 0},
    {Xmm10, "xmm10", 0},
    {Xmm11, "xmm11", 0},
    {Xmm12, "xmm12", 0},
    {Xmm13, "xmm13", 0},
    {Xmm14, "xmm14", 0},
    {Xmm15, "xmm15", 0},
};

u8 Register_get_addreg_code(Register self) {
    for(u32 i=0; i<LEN(REGISTER_TABLE); i++) {
        Register rtable_reg = REGISTER_TABLE[i].reg;
        if(rtable_reg == self) {
            return REGISTER_TABLE[i].addreg_code;
        }
    }
    return 0;
}

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

