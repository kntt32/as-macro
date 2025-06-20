#pragma once

#include "types.h"
#include "parser.h"

#define ADDREG_ADDCODE 0x7
#define ADDREG_REXB 0x8
#define ADDREG_REX 0x80

#define MODRMREG_REGFIELD 0x7
#define MODRMREG_REXR 0x8
#define MODRMREG_REX 0x80

#define MODRMBASE_RMFIELD 0x7
#define MODRMBASE_REXB 0x8
#define MODRMREG_REX 0x80

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

SResult Register_get_addreg_code(Register self, out u8* value);

SResult Register_get_modrmreg_code(Register self, out u8* value);

SResult Register_get_modrmregmem_base_code(Register self, out u8* value);

ParserMsg Register_parse(Parser* parser, Register* ptr);

void Register_print(in Register self);

bool Register_is_integer(Register self);

bool Register_is_xmm(Register self);


