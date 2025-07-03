#include <stdio.h>
#include "types.h"
#include "register.h"

static struct {
    Register reg;
    char* str;
    u8 addreg_code;
    u8 modrmreg_code;
    u8 modrmregmem_reg_code;
} REGISTER_TABLE[] = {
    {Rsp, "rsp", 0x80+4, 0x80+4, 0x80+4},
    {Rbp, "rbp", 0x80+5, 0x80+5, 0x80+5},

    {Rax, "rax", 0, 0, 0},
    {Rcx, "rcx", 1, 1, 1},
    {Rdx, "rdx", 2, 2, 2},
    {Rbx, "rbx", 3, 3, 3},
    {Rsi, "rsi", 0x80+6, 0x80+6, 0x80+6},
    {Rdi, "rdi", 0x80+7, 0x80+7, 0x80+7},
    {R8, "r8", 8, 8, 8},
    {R9, "r9", 9, 9, 9},
    {R10, "r10", 10, 10, 10},
    {R11, "r11", 11, 11, 11},
    {R12, "r12", 12, 12, 12},
    {R13, "r13", 13, 13, 13},
    {R14, "r14", 14, 14, 14},
    {R15, "r15", 15, 15, 15},
};

static struct {Register reg; char* str;} XMM_TABLE[] = {
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

SResult Register_get_addreg_code(Register self, out u8* value) {
    for(u32 i=0; i<LEN(REGISTER_TABLE); i++) {
        if(REGISTER_TABLE[i].reg == self) {
            *value = REGISTER_TABLE[i].addreg_code;
            return SResult_new(NULL);
        }
    }

    return SResult_new("unexpected register");
}

SResult Register_get_reg_code(Register self, out u8* value) {
    for(u32 i=0; i<LEN(REGISTER_TABLE); i++) {
        if(REGISTER_TABLE[i].reg == self) {
            *value = REGISTER_TABLE[i].modrmreg_code;
            return SResult_new(NULL);
        }
    }
    
    return SResult_new("unexpected register");
}

SResult Register_get_modrm_base_code(Register self, out u8* value) {
    for(u32 i=0; i<LEN(REGISTER_TABLE); i++) {
        if(REGISTER_TABLE[i].reg == self) {
            *value = REGISTER_TABLE[i].modrmregmem_reg_code;
            return SResult_new(NULL);
        }
    }

    return SResult_new("unexpected register");
}

ParserMsg Register_parse(Parser* parser, Register* restrict ptr) {
    if(ParserMsg_is_success(Parser_parse_keyword(parser, "rip"))) {
        *ptr = Rip;
        return ParserMsg_new(parser->offset, NULL);
    }

    for(u32 i=0; i<LEN(REGISTER_TABLE); i++) {
        char* rtable_str = REGISTER_TABLE[i].str;
        Register rtable_reg = REGISTER_TABLE[i].reg;

        if(ParserMsg_is_success(Parser_parse_keyword(parser, rtable_str))) {
            *ptr = rtable_reg;
            return ParserMsg_new(parser->offset, NULL);
        }
    }

    for(u32 i=0; i<LEN(XMM_TABLE); i++) {
        char* rtable_str = XMM_TABLE[i].str;
        Register rtable_reg = XMM_TABLE[i].reg;

        if(Parser_parse_keyword(parser, rtable_str).msg[0] == '\0') {
            *ptr = rtable_reg;
            return ParserMsg_new(parser->offset, NULL);
        } 
    }

    return ParserMsg_new(parser->offset, "expected keyword");
}

bool Register_is_integer(Register self) {
    for(u32 i=0; i<LEN(REGISTER_TABLE); i++) {
        if(REGISTER_TABLE[i].reg == self) {
            return true;
        }
    }
    return false;
}

bool Register_is_xmm(Register self) {
    for(u32 i=0; i<LEN(XMM_TABLE); i++) {
        if(XMM_TABLE[i].reg == self) {
            return true;
        }
    }
    return false;
}

void Register_print(in Register self) {
    if(self == Rip) {
        printf("rip");
        return;
    }

    for(u32 i=0; i<LEN(REGISTER_TABLE); i++) {
        char* rtable_str = REGISTER_TABLE[i].str;
        Register rtable_reg = REGISTER_TABLE[i].reg;

        if(self == rtable_reg) {
            printf("%s", rtable_str);
            return;
        }
    }

    for(u32 i=0; i<LEN(XMM_TABLE); i++) {
        char* xtable_str = XMM_TABLE[i].str;
        Register xtable_reg = XMM_TABLE[i].reg;

        if(self == xtable_reg) {
            printf("%s", xtable_str);
            return;
        }
    }

    PANIC("unknown value");
    
    return;
}

