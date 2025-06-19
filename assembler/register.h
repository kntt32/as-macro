#pragma once

#include "types.h"
#include "parser.h"

#define ADDREG_ADDCODE 0x7
#define ADDREG_REXB 0x8
#define ADDREG_REX 0x80

typedef enum {
    Rsp,
    Rbp,

    Rax,
    Rcx,
    Rdx,
    Rbx,
    Rsi,
    Rdi,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,

    Xmm0,
    Xmm1,
    Xmm2,
    Xmm3,
    Xmm4,
    Xmm5,
    Xmm6,
    Xmm7,
    Xmm8,
    Xmm9,
    Xmm10,
    Xmm11,
    Xmm12,
    Xmm13,
    Xmm14,
    Xmm15,
} Register;

u8 Register_get_addreg_code(Register self);

ParserMsg Register_parse(Parser* parser, Register* ptr);

void Register_print(in Register self);

