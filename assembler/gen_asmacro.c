#include <stdio.h>
#include <assert.h>
#include "gen.h"

static ParserMsg Argument_parse_storage_bound(inout Parser* parser, inout Argument* argument) {
    Parser parser_copy = *parser;

    if(!Parser_start_with(parser, "imm") && ParserMsg_is_success(Storage_parse(&parser_copy, 0, &argument->storage.storage))) {
        argument->storage_type = Argument_Storage;
    }else {
        struct { char* keyword; bool* flag;} bounds[] = {
            {"reg", &argument->storage.trait.reg_flag},
            {"mem", &argument->storage.trait.mem_flag},
            {"imm", &argument->storage.trait.imm_flag}
        };
        
        for(u32 i=0; i<3; i++) {
            *bounds[i].flag = false;
        }

        do {
            bool flag = false;
            for(u32 i=0; i<LEN(bounds); i++) {
                if(ParserMsg_is_success(Parser_parse_keyword(&parser_copy, bounds[i].keyword))) {
                    *bounds[i].flag = true;
                    flag = true;
                    break;
                }
            }
            if(!flag) {
                return ParserMsg_new(parser->offset, "expected storage bound");
            }
        } while(ParserMsg_is_success(Parser_parse_symbol(&parser_copy, "+")));

        argument->storage_type = Argument_Trait;
    }

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Argument_parse(inout Parser* parser, in Generator* generator, out Argument* argument) {
    // (in)(out) name : Type @ [reg | mem | xmm | imm]
    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, argument->name),
        (void)NULL
    );
    
    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, ":"),
        (void)NULL
    );
    PARSERMSG_UNWRAP(
        Type_parse(&parser_copy, generator, &argument->type),
        (void)NULL
    );

    PARSERMSG_UNWRAP(
        Parser_parse_symbol(&parser_copy, "@"),
        Argument_free(*argument)
    );
    PARSERMSG_UNWRAP(
        Argument_parse_storage_bound(&parser_copy, argument),
        Argument_free(*argument)
    );

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

bool Argument_cmp(in Argument* self, out Argument* other) {
    if(!Type_cmp(&self->type, &other->type)) {
        return false;
    }
    if(self->storage_type != other->storage_type) {
        return false;
    }
    switch(self->storage_type) {
        case Argument_Trait:    
            if(self->storage.trait.reg_flag != other->storage.trait.reg_flag
                || self->storage.trait.mem_flag != other->storage.trait.mem_flag) {
                return false;
            }
            break;
        case Argument_Storage:
            if(!Storage_cmp(&self->storage.storage, &other->storage.storage)) {
                return false;
            }
            break;
    }

    return strcmp(self->name, other->name);
}

bool Argument_cmp_for_vec(in void* self, in void* other) {
    return Argument_cmp(self, other);
}

Argument Argument_from(in Variable* variable) {
    Argument argument;
    strcpy(argument.name, variable->name);
    argument.type = Type_clone(&variable->data.type);

    Storage* var_storage = &variable->data.storage;
    switch(var_storage->type) {
        case StorageType_imm:
        case StorageType_reg:
        case StorageType_xmm:
            argument.storage_type = Argument_Storage;
            argument.storage.storage = Storage_clone(var_storage);
            break;
        case StorageType_mem:
            argument.storage_type = Argument_Trait;
            argument.storage.trait.reg_flag = false;
            argument.storage.trait.mem_flag = true;
            argument.storage.trait.imm_flag = true;
            argument.storage.trait.xmm_flag = false;
            break;
    }

    return argument;
}

bool Argument_match_with(in Argument* self, in Data* data) {
    Storage storage = data->storage;
    switch(self->storage_type) {
        case Argument_Trait:
            if(!((self->storage.trait.reg_flag && storage.type == StorageType_reg)
                || (self->storage.trait.mem_flag && storage.type == StorageType_mem)
                || (self->storage.trait.imm_flag && storage.type == StorageType_imm)
                || (self->storage.trait.xmm_flag && storage.type == StorageType_xmm))) {
                return false;
            }
            if(data->storage.type == StorageType_imm && (self->type.type == Type_Integer || self->type.type == Type_Floating) && self->type.type == data->type.type) {
                if(data->storage.body.imm.type != Imm_Label) {
                    return true;
                }
            }
            break;
        case Argument_Storage:
            if(!Storage_cmp(&storage, &self->storage.storage)) {
                return false;
            }
            break;
    }

    if(!Type_cmp(&self->type, &data->type)) {
        return false;
    }

   return true;
}

bool Argument_match_self(in Argument* self, in Argument* other) {
    if(!Type_cmp(&self->type, &other->type)) {
        return false;
    }

    switch(self->storage_type) {
        case Argument_Trait:
            switch(other->storage_type) {
            case Argument_Trait:
                if((!self->storage.trait.reg_flag && other->storage.trait.reg_flag)
                    || (!self->storage.trait.mem_flag && other->storage.trait.mem_flag)
                    || (!self->storage.trait.imm_flag && other->storage.trait.imm_flag)
                    || (!self->storage.trait.xmm_flag && other->storage.trait.xmm_flag)) {
                    return false;
                }
                break;
            case Argument_Storage:
            {
                Storage storage = other->storage.storage;
                if((!self->storage.trait.reg_flag && storage.type == StorageType_reg)
                    || (!self->storage.trait.mem_flag && storage.type == StorageType_mem)
                    || (!self->storage.trait.imm_flag && storage.type == StorageType_imm)
                    || (!self->storage.trait.xmm_flag && storage.type == StorageType_xmm)) {
                    return false;
                }
            }
                break;
            }
            break;
        case Argument_Storage:
            switch(other->storage_type) {
            case Argument_Trait:
                return false;
            case Argument_Storage:
                if(!Storage_cmp(&self->storage.storage, &other->storage.storage)) {
                    return false;
                }
                break;
            }
            break;
    }

    return true;
}

void Argument_print(in Argument* self) {
    printf("Argument { name: %s, type: ", self->name);
    Type_print(&self->type);
    printf(", storage_type: %d, storage: ", self->storage_type);
    switch(self->storage_type) {
        case Argument_Trait:
            printf(".trait: { reg_flag: %s, mem_flag: %s, imm_flag: %s }",
                BOOL_TO_STR(self->storage.trait.reg_flag),
                BOOL_TO_STR(self->storage.trait.mem_flag),
                BOOL_TO_STR(self->storage.trait.imm_flag));
            break;
        case Argument_Storage:
            printf(".storage: ");
            Storage_print(&self->storage.storage);
            break;
    }
    printf(" }");
}

void Argument_print_for_vec(in void* ptr) {
    Argument_print(ptr);
}

Argument Argument_clone(in Argument* self) {
    Argument argument;

    strcpy(argument.name, self->name);
    argument.type = Type_clone(&self->type);
    argument.storage_type = self->storage_type;
    switch(self->storage_type) {
        case Argument_Trait:
            argument.storage.trait = self->storage.trait;
            break;
        case Argument_Storage:
            argument.storage.storage = self->storage.storage;
            break;
    }

    return argument;
}

void Argument_clone_for_vec(in void* src, out void* dst) {
    Argument* src_ptr = src;
    *src_ptr = Argument_clone(dst);
}

void Argument_free(Argument self) {
    Type_free(self.type);
}

void Argument_free_for_vec(inout void* ptr) {
    Argument* argument = ptr;
    Argument_free(*argument);
}

static SResult AsmArgs_from_trait(in Data* data, in Argument* arg, inout AsmArgs* asm_args) {
    SResult result = SResult_new("found unsupported storage bound");

    bool reg_flag = arg->storage.trait.reg_flag;
    bool mem_flag = arg->storage.trait.mem_flag;
    bool imm_flag = arg->storage.trait.imm_flag;
    bool xmm_flag = arg->storage.trait.xmm_flag;

    switch(data->storage.type) {
        case StorageType_reg:
            if(reg_flag && mem_flag && !imm_flag && !xmm_flag) {
                asm_args->regmem_flag = true;
                asm_args->regmem_type = AsmArgs_Rm_Reg;
                asm_args->regmem.reg = data->storage.body.reg;
            }else if(reg_flag && !mem_flag && !imm_flag && !xmm_flag) {
                asm_args->reg_flag = true;
                asm_args->reg_type = AsmArgs_Reg_Reg;
                asm_args->reg.reg = data->storage.body.reg;
            }else {
                return result;
            }
            break;
        case StorageType_mem:
            asm_args->regmem_flag = true;
            asm_args->regmem_type = AsmArgs_Rm_Mem;
            if((reg_flag && mem_flag && !imm_flag && !xmm_flag)
                || (!reg_flag && mem_flag && !imm_flag && xmm_flag)
                || (!reg_flag && mem_flag && !imm_flag && !xmm_flag)) {
                asm_args->regmem_flag = true;
                asm_args->regmem_type = AsmArgs_Rm_Mem;
                asm_args->regmem.mem = data->storage.body.mem;
            }else {
                return result;
            }
            break;
        case StorageType_xmm:
            if(!reg_flag && mem_flag && !imm_flag && xmm_flag) {
                asm_args->regmem_flag = true;
                asm_args->regmem_type = AsmArgs_Rm_Xmm;
                asm_args->regmem.xmm = data->storage.body.xmm;
            }else if(!reg_flag && !mem_flag && !imm_flag && xmm_flag) {
                asm_args->reg_flag = true;
                asm_args->reg_type = AsmArgs_Reg_Xmm;
                asm_args->reg.xmm = data->storage.body.xmm;
            }else {
                return result;
            }
            break;
        case StorageType_imm:
            if(!reg_flag && !mem_flag && imm_flag && !xmm_flag) {
                asm_args->imm_flag = true;
                asm_args->imm = Imm_clone(&data->storage.body.imm);
            }else {
                return result;
            }
            break;
    }
    
    return SResult_new(NULL);
}

SResult AsmArgs_from(in Vec* dataes, in Vec* arguments, out AsmArgs* asm_args) {
    asm_args->reg_flag = false;
    asm_args->regmem_flag = false;
    asm_args->imm_flag = false;

    for(u32 i=0; i<Vec_len(arguments); i++) {
        Argument* arg = Vec_index(arguments, i);
        Data* data = Vec_index(dataes, i);

        if(!Argument_match_with(arg, data)) {
            PANIC("mismatch with dataes and arguments");
        }
        switch(arg->storage_type) {
            case Argument_Trait:
                SRESULT_UNWRAP(
                    AsmArgs_from_trait(data, arg, asm_args),
                    (void)NULL
                );
                break;
            case Argument_Storage:
                break;
        }
    }

    return SResult_new(NULL);
}

AsmArgs AsmArgs_clone(in AsmArgs* self) {
    AsmArgs asm_args = *self;

    if(self->imm_flag) {
        asm_args.imm = Imm_clone(&self->imm);
    }

    return asm_args;
}

void AsmArgs_free(AsmArgs self) {
    if(self.imm_flag) {
        Imm_free(self.imm);
    }
}

static ParserMsg Asmacro_parse_header_arguments(Parser parser, in Generator* generator, inout Vec* arguments) {
    while(!Parser_is_empty(&parser)) {
        Argument argument;
        PARSERMSG_UNWRAP(
            Argument_parse(&parser, generator, &argument),
            (void)NULL
        );
        Vec_push(arguments, &argument);

        if(!Parser_is_empty(&parser)) {
            if(!ParserMsg_is_success(Parser_parse_symbol(&parser, ","))) {
                return ParserMsg_new(parser.offset, "expected symbol \",\"");
            }
        }
    }

    return ParserMsg_new(parser.offset, NULL);
}

static ParserMsg Asmacro_parse_header(inout Parser* parser, in Generator* generator, out Asmacro* asmacro) {
    Parser parser_copy = *parser;

    if(ParserMsg_is_success(Parser_parse_keyword(&parser_copy, "pub"))) {
        asmacro->valid_path[0] = '\0';
    }else {
        strcpy(asmacro->valid_path, Parser_path(parser));
    }

    PARSERMSG_UNWRAP(
        Parser_parse_keyword(&parser_copy, "as"),
        (void)NULL
    );
    
    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, asmacro->name),
        (void)NULL
    );
    
    Parser block_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser_copy, &block_parser),
        (void)NULL
    );
    asmacro->arguments = Vec_new(sizeof(Argument));
    PARSERMSG_UNWRAP(
        Asmacro_parse_header_arguments(block_parser, generator, &asmacro->arguments),
        Vec_free_all(asmacro->arguments, Argument_free_for_vec)
    );

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

static ParserMsg Asmacro_parse_proc(inout Parser* parser, out Asmacro* asmacro) {
    Parser block_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_block(parser, &block_parser),
        (void)NULL
    );
    
    asmacro->type = Asmacro_UserOperator;
    asmacro->body.user_operator = block_parser;

    return ParserMsg_new(parser->offset, NULL);
}

static ParserMsg Asmacro_parse_encoding(inout Parser* parser, out Asmacro* asmacro) {
    Parser parser_copy = *parser;

    Parser paren_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser_copy, &paren_parser),
        (void)NULL
    );

    AsmArgSize sizes;
    PARSERMSG_UNWRAP(
        ParserMsg_from_sresult(
            AsmArgSize_from(&asmacro->arguments, &sizes),
            parser->offset
        ),
        (void)NULL
    );

    asmacro->type = Asmacro_AsmOperator;
    PARSERMSG_UNWRAP(
        AsmEncoding_parse(&paren_parser, &sizes, &asmacro->body.asm_operator),
        (void)NULL
    );

    if(Vec_len(&asmacro->arguments) != 0) {
        Argument* argument = Vec_index(&asmacro->arguments, 0);
        switch(argument->type.size) {
            case 1:
            case 2:
            case 4:
            case 8:
                break;
            default:
                AsmEncoding_free(asmacro->body.asm_operator);
                return ParserMsg_new(parser->offset, "operand size must be 1, 2, 4 or 8 byte");
        }
        asmacro->body.asm_operator.operand_size = argument->type.size * 8;
    }

    *parser = parser_copy;

    return ParserMsg_new(parser->offset, NULL);
}

ParserMsg Asmacro_parse(inout Parser* parser, in Generator* generator, out Asmacro* asmacro) {
    // as name ( args ) { proc }
    // as name ( args ) : ( encoding )
    assert(parser != NULL && generator != NULL && asmacro != NULL);

    Parser parser_copy = *parser;

    PARSERMSG_UNWRAP(
        Asmacro_parse_header(&parser_copy, generator, asmacro),
        (void)NULL
    );

    if(ParserMsg_is_success(Parser_parse_symbol(&parser_copy, ":"))) {
        PARSERMSG_UNWRAP(
            Asmacro_parse_encoding(&parser_copy, asmacro),
            Vec_free_all(asmacro->arguments, Argument_free_for_vec)
        );
    }else {
        PARSERMSG_UNWRAP(
            Asmacro_parse_proc(&parser_copy, asmacro),
            Vec_free_all(asmacro->arguments, Argument_free_for_vec)
        );
    }

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

Asmacro Asmacro_new_fn_wrapper(in char* name, Vec arguments/* Vec<Variable> */, in char* valid_path) {
    assert(name != NULL);
    assert(valid_path != NULL);
    assert(Vec_size(&arguments) == sizeof(Variable));

    Asmacro asmacro;

    strcpy(asmacro.name, name);
    strcpy(asmacro.valid_path, valid_path);

    asmacro.arguments = Vec_new(sizeof(Argument));
    for(u32 i=0; i<Vec_len(&arguments); i++) {
        Variable* variable = Vec_index(&arguments, i);
        Argument arg = Argument_from(variable);
        Vec_push(&asmacro.arguments, &arg);
    }
    
    asmacro.type = Asmacro_FnWrapper;
    asmacro.body.fn_wrapper = arguments;

    return asmacro;
}

SResult Asmacro_match_with(in Asmacro* self, in Vec* dataes, in char* path) {
    assert(self != NULL);
    assert(dataes != NULL);
    assert(path != NULL);

    if(self->valid_path[0] != '\0' && strcmp(self->valid_path, path) != 0) {
        return SResult_new("out of namespace");
    }

    if(Vec_len(&self->arguments) != Vec_len(dataes)) {
        return SResult_new("mismatching the number of arguments");
    }
    
    for(u32 i=0; i<Vec_len(&self->arguments); i++) {
        Argument* argument = Vec_index(&self->arguments, i);
        Data* data = Vec_index(dataes, i);
        if(!Argument_match_with(argument, data)) {
            char msg[256];
            snprintf(msg, 256, "mismatchig argument%u", i+1);
            return SResult_new(msg);
        }
    }

    return SResult_new(NULL);
}

bool Asmacro_cmp_signature(in Asmacro* self, in Asmacro* other) {
    return strcmp(self->name, other->name) == 0
        && Vec_cmp(&self->arguments, &other->arguments, Argument_cmp_for_vec);
}

void Asmacro_print(in Asmacro* self) {
    assert(self != NULL);

    printf("Asmacro { name: %s, valid_path: %s, arguments: ", self->name, self->valid_path);
    Vec_print(&self->arguments, Argument_print_for_vec);
    printf(", type: %d, body: ", self->type);

    switch(self->type) {
        case Asmacro_AsmOperator:
            printf(".asm_operator: ");
            AsmEncoding_print(&self->body.asm_operator);
            break;
        case Asmacro_UserOperator:
            printf(".user_operator: ");
            Parser_print(&self->body.user_operator);
            break;
        case Asmacro_FnWrapper:
            printf(".fn_wrapper: ");
            Vec_print(&self->body.fn_wrapper, Variable_print_for_vec);
            break;
    }
    
    printf(" }");
}

void Asmacro_print_for_vec(in void* ptr) {
    Asmacro_print(ptr);
}

Asmacro Asmacro_clone(in Asmacro* self) {
    Asmacro asmacro;

    strcpy(asmacro.name, self->name);
    strcpy(asmacro.valid_path, self->valid_path);
    asmacro.arguments = Vec_clone(&self->arguments, Argument_clone_for_vec);
    asmacro.type = self->type;

    switch(self->type) {
        case Asmacro_AsmOperator:
            asmacro.body.asm_operator = AsmEncoding_clone(&self->body.asm_operator);
            break;
        case Asmacro_UserOperator:
            asmacro.body.user_operator = self->body.user_operator;
            break;
        case Asmacro_FnWrapper:
            asmacro.body.fn_wrapper = Vec_clone(&self->body.fn_wrapper, Variable_clone_for_vec);
            break;
    }

    return asmacro;
}

void Asmacro_free(Asmacro self) {
    Vec_free_all(self.arguments, Argument_free_for_vec);
    switch(self.type) {
        case Asmacro_AsmOperator:
            AsmEncoding_free(self.body.asm_operator);
            break;
        case Asmacro_UserOperator:
            break;
        case Asmacro_FnWrapper:
            Vec_free_all(self.body.fn_wrapper, Variable_free_for_vec);
            break;
    }
}

void Asmacro_free_for_vec(inout void* ptr) {
    Asmacro* asmacro = ptr;
    Asmacro_free(*asmacro);
}
