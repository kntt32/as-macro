#pragma once
#include "types.h"
#include "vec.h"
#include "parser.h"

typedef struct {
    enum {
        ModRmType_R,
        ModRmType_Dight,
    } type;
    union {
        u8 dight;// 0~7
    } body;
} ModRmType;

typedef struct {
    enum {
        AsmEncodingElement_Value,
        AsmEncodingElement_Imm,
        AsmEncodingElement_ModRm,
        AsmEncodingElement_AddReg,
    } type;
    union {
        u8 value;
        u8 imm;// size of immidiate value (8, 16, 32, 64)
        ModRmType mod_rm;// modrm type
        u8 add_reg;// register size to add (8, 16, 32, 64)
    } body;
} AsmEncodingElement;

typedef struct {
    Vec encoding_elements;// Vec<AsmEncodingElement>
    u8 default_operand_size;// 32, 64
} AsmEncoding;

ParserMsg ModRmType_parse(inout Parser* parser, out ModRmType* mod_rm_type);
void ModRmType_print(in ModRmType* self);

ParserMsg AsmEncodingElement_parse(inout Parser* parser, out AsmEncodingElement* asm_encoding_element);
void AsmEncodingElement_print(in AsmEncodingElement* self);
void AsmEncodingElement_print_for_vec(in void* ptr);

ParserMsg AsmEncoding_parse(Parser parser, in AsmEncoding* asm_encoding);
void AsmEncoding_print(in AsmEncoding* self);
void AsmEncoding_free(AsmEncoding self);

