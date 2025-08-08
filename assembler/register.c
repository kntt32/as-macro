#include <stdio.h>
#include <stdbool.h>
#include "types.h"
#include "register.h"

Register GENERAL_REGISTERS[14] = {
    R8, R9, R10, R11, R12, R13, R14, R15, Rax, Rcx, Rdx, Rbx, Rsi, Rdi
};

static struct {
    Register reg;
    char* str;
    u8 addreg_code;
    u8 modrmreg_code;
    u8 modrmregmem_reg_code;
    bool volatile_flag;
} REGISTER_TABLE[] = {
    {Rsp, "rsp", 0x80+4, 0x80+4, 0x80+4, false},
    {Rbp, "rbp", 0x80+5, 0x80+5, 0x80+5, false},

    {Rax, "rax", 0, 0, 0, true},
    {Rcx, "rcx", 1, 1, 1, true},
    {Rdx, "rdx", 2, 2, 2, true},
    {Rbx, "rbx", 3, 3, 3, false},
    {Rsi, "rsi", 0x80+6, 0x80+6, 0x80+6, true},
    {Rdi, "rdi", 0x80+7, 0x80+7, 0x80+7, true},
    {R8, "r8", 8, 8, 8, true},
    {R9, "r9", 9, 9, 9, true},
    {R10, "r10", 10, 10, 10, true},
    {R11, "r11", 11, 11, 11, true},
    {R12, "r12", 12, 12, 12, false},
    {R13, "r13", 13, 13, 13, false},
    {R14, "r14", 14, 14, 14, false},
    {R15, "r15", 15, 15, 15, false},
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

bool Register_is_volatile(Register self) {
    for(u32 i=0; i<LEN(REGISTER_TABLE); i++) {
        if(REGISTER_TABLE[i].reg == self) {
            return REGISTER_TABLE[i].volatile_flag;
        }
    }

    return false;
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
    
    return ParserMsg_new(parser->offset, "expected register");
}

bool Register_is_integer(Register self) {
    for(u32 i=0; i<LEN(REGISTER_TABLE); i++) {
        if(REGISTER_TABLE[i].reg == self) {
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
    
    PANIC("unknown value");
    
    return;
}

