#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "types.h"
#include "gen.h"
#include "syntax.h"
#include "util.h"

static SResult sub_rsp(i32 value, inout Generator* generator, inout VariableManager* variable_manager);

void StoredReg_print(in StoredReg* self) {
    printf("StoredReg { reg: ");
    Register_print(self->reg);
    printf(", stack_offset: %d }", self->stack_offset);
}

void StoredReg_print_for_vec(in void* ptr) {
    StoredReg_print(ptr);
}

void VariableBlock_print(in VariableBlock* self) {
    printf("VariableBlock { stack_offset: %d, variables_base: %u, stored_regs: ", self->stack_offset, self->variables_base);
    Vec_print(&self->stored_regs, StoredReg_print_for_vec);
    printf(" }");
}

void VariableBlock_print_for_vec(in void* ptr) {
    VariableBlock_print(ptr);
}

void VariableBlock_free(VariableBlock self) {
    Vec_free(self.stored_regs);
}

void VariableBlock_free_for_vec(inout void* ptr) {
    VariableBlock* self = ptr;
    VariableBlock_free(*self);
}

VariableManager VariableManager_new(i32 stack_offset) {
    VariableManager variable_manager = {
        Vec_new(sizeof(Variable)),
        stack_offset,
        Vec_new(sizeof(VariableBlock))
    };
    VariableBlock block = {
        stack_offset,
        0,
        Vec_new(sizeof(StoredReg))
    };
    Vec_push(&variable_manager.blocks, &block);

    return variable_manager;
}

i32* VariableManager_stack_offset(in VariableManager* self) {
    return &self->stack_offset;
}

static VariableBlock* get_last_block(in VariableManager* self) {
    VariableBlock* block = Vec_index(&self->blocks, Vec_len(&self->blocks)-1);
    return block;
}

static SResult store_reg(inout VariableManager* self, Register reg, inout Generator* generator) {
    Data reg_data = Data_from_register(reg);
    Vec push_arg = Vec_new(sizeof(Data));
    Vec_push(&push_arg, &reg_data);

    Data data;
    SRESULT_UNWRAP(
        expand_asmacro("push", "", push_arg, generator, self, &data),
        (void)NULL
    );
    Data_free(data);

    self->stack_offset -= 8;
    VariableBlock* last_block = get_last_block(self);
    StoredReg stored_reg = {reg, self->stack_offset};
    Vec_push(&last_block->stored_regs, &stored_reg);

    return SResult_new(NULL);
}

static SResult store_variable(inout VariableManager* self, in Variable* variable, inout Generator* generator) {
    Storage* storage = &variable->data.storage;
   
    Register reg;
    switch(storage->type) {
        case StorageType_reg:
            reg = storage->body.reg;
            break;
        case StorageType_xmm:
            reg = storage->body.xmm;
            break;
        default:
            return SResult_new(NULL);
    }

    SRESULT_UNWRAP(
        store_reg(self, reg, generator),
        (void)NULL
    );
    
    return SResult_new(NULL);
}

static bool is_stored(in VariableManager* self, in Variable* variable, u32 variable_index) {
    for(u32 i=variable_index+1; i<Vec_len(&self->variables); i++) {
        Variable* ptr = Vec_index(&self->variables, i);
        if(Storage_cmp(&variable->data.storage, &ptr->data.storage)) {
            return true;
        }
    }

    return false;
}

SResult VariableManager_push(inout VariableManager* self, Variable variable, inout Generator* generator) {
    VariableBlock* block = get_last_block(self);
    u32 variable_base = block->variables_base;
    
    for(i32 i=Vec_len(&self->variables)-1; 0<=i; i--) {
        Variable* ptr = Vec_index(&self->variables, i);
        
        bool doubling_storage_flag = Storage_cmp(&ptr->data.storage, &variable.data.storage);

        if(doubling_storage_flag) {
            if((i32)variable_base <= i) {
                ptr->name[0] = '\0';
                break;
            }else {
                SRESULT_UNWRAP(
                    store_variable(self, ptr, generator),
                    (void)NULL
                );
                break;
            }
        }

        if(i == 0) {
            Storage* storage = &variable.data.storage;
            Register reg;
            switch(storage->type) {
                case StorageType_reg:
                    reg = storage->body.reg;
                    break;
                case StorageType_xmm:
                    reg = storage->body.xmm;
                    break;
                default:
                    continue;
            }
            if(!Register_is_volatile(reg)) {
                SRESULT_UNWRAP(
                    store_reg(self, reg, generator),
                    (void)NULL
                );
            }
        }
    }

    Vec_push(&self->variables, &variable);

    return SResult_new(NULL);
}

void VariableManager_push_alias(inout VariableManager* self, Variable variable) {
    assert(self != NULL);
    Vec_push(&self->variables, &variable);
}

SResult VariableManager_get(inout VariableManager* self, in char* name, out Variable* variable) {
    for(i32 i=Vec_len(&self->variables)-1; 0<=i; i--) {
        Variable* ptr = Vec_index(&self->variables, i);
        if(strcmp(ptr->name, name) == 0 && !is_stored(self, ptr, i)) {
            *variable = Variable_clone(ptr);
            return SResult_new(NULL);
        }
    }
    
    char msg[256];
    snprintf(msg, 256, "variable \"%.10s\" is undefiend", name);
    return SResult_new(msg);
}

void VariableManager_new_block(inout VariableManager* self) {
    assert(self != NULL);

    VariableBlock block = {
        self->stack_offset, Vec_len(&self->variables), Vec_new(sizeof(StoredReg))
    };

    Vec_push(&self->blocks, &block);
}

static SResult restore_variable(in StoredReg* stored_reg, inout Generator* generator, inout VariableManager* variable_manager) {
    Vec mov_args = Vec_new(sizeof(Data));
    Type type = {"i64", "", Type_Integer, {}, 8, 8};
    Data mov_src = Data_from_mem(Rbp, stored_reg->stack_offset, type);
    Data mov_dst = Data_from_register(stored_reg->reg);
    Vec_push(&mov_args, &mov_src);
    Vec_push(&mov_args, &mov_dst);

    Data data;
    SRESULT_UNWRAP(
        expand_asmacro("mov", "", mov_args, generator, variable_manager, &data),
        (void)NULL
    );

    Data_free(data);
    return SResult_new(NULL);
}

SResult VariableManager_delete_block(inout VariableManager* self, inout Generator* generator) {
    assert(self != NULL && generator != NULL);

    VariableBlock block;
    Vec_pop(&self->blocks, &block);

    for(i32 i=Vec_len(&self->variables)-1; (i32)block.variables_base<=i; i--) {
        Variable variable;
        Vec_pop(&self->variables, &variable);
        Variable_free(variable);
    }
    assert(Vec_len(&self->variables) == block.variables_base);

    for(i32 i=Vec_len(&block.stored_regs)-1; 0<=i; i--) {
        StoredReg* stored_reg = Vec_index(&block.stored_regs, i);
        SRESULT_UNWRAP(
            restore_variable(stored_reg, generator, self),
            VariableBlock_free(block)
        );
    }

    SRESULT_UNWRAP(
        sub_rsp(self->stack_offset - block.stack_offset, generator, self),
        VariableBlock_free(block)
    );
    self->stack_offset = block.stack_offset;
    VariableBlock_free(block);

    return SResult_new(NULL);
}

void VariableManager_print(in VariableManager* self) {
    printf("VariableManager { variables: ");
    Vec_print(&self->variables, Variable_print_for_vec);
    printf(", stack_offset: %d, blocks: ", self->stack_offset);
    Vec_print(&self->blocks, VariableBlock_print_for_vec);
}

void VariableManager_print_for_vec(in void* ptr) {
    VariableManager_print(ptr);
}

void VariableManager_free(VariableManager self) {
    Vec_free_all(self.variables, Variable_free_for_vec);
    Vec_free_all(self.blocks, VariableBlock_free_for_vec);
}

void VariableManager_free_for_vec(inout void* ptr) {
    VariableManager* variable_manager = ptr;
    VariableManager_free(*variable_manager);
}

void check_parser(in Parser* parser, inout Generator* generator) {
    if(!Parser_is_empty(parser)) {
        Generator_add_error(generator, Error_new(parser->offset, "unexpected token"));
    }
}

SResult Syntax_build(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    bool (*BUILDERS[])(Parser, inout Generator*, inout VariableManager* variable_manager, out Data*) = {
        Syntax_build_variable_declaration,
        Syntax_build_return,
        Syntax_build_asmacro_expansion,
        Syntax_build_register_expression,
        Syntax_build_imm_expression,
        Syntax_build_true_expression,
        Syntax_build_false_expression,
        Syntax_build_null_expression,
        Syntax_build_char_expression,
        Syntax_build_assignment,
        Syntax_build_dot_operator,
        Syntax_build_refer_operator,
        Syntax_build_subscript_operator,
        Syntax_build_sizeof_operator,
        Syntax_build_alignof_operator,
        Syntax_build_variable_expression,
    };
    
    for(u32 i=0; i<LEN(BUILDERS); i++) {
        Parser parser_copy = parser;
        if(BUILDERS[i](parser_copy, generator, variable_manager, data)) {
            return SResult_new(NULL);
        }
    }

    return SResult_new("unknown expression");
}

static SResult Syntax_build_asmacro_expansion_get_arguments(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Vec* arguments) {
    *arguments = Vec_new(sizeof(Data));

    while(!Parser_is_empty(&parser)) {
        Parser arg_parser = Parser_split(&parser, ",");
        
        Data arg;
        SRESULT_UNWRAP(
            Syntax_build(arg_parser, generator, variable_manager, &arg),
            Vec_free_all(*arguments, Data_free_for_vec)
        );

        Vec_push(arguments, &arg);
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_search_asmacro(in Vec* asmacroes, in Vec* arguments, in char* path, out Asmacro* asmacro) {
    assert(asmacroes != NULL);
    assert(arguments != NULL);
    assert(path != NULL);
    assert(asmacro != NULL);

    for(u32 i=0; i<Vec_len(asmacroes); i++) {
        Asmacro* ptr = Vec_index(asmacroes, i);
        if(SResult_is_success(Asmacro_match_with(ptr, arguments, path))) {
            *asmacro = Asmacro_clone(ptr);
            return SResult_new(NULL);
        }
    }

    return SResult_new("mismatching asmacro");
}

static SResult Syntax_build_asmacro_expansion_get_info(
    in char* name,
    in char* path,
    inout Generator* generator,
    in Vec* arguments,
    out Asmacro* asmacro
) {
    Vec asmacroes;
    SRESULT_UNWRAP(
        Generator_get_asmacro(generator, name, &asmacroes),
        (void)NULL
    );

    SRESULT_UNWRAP(
        Syntax_build_asmacro_expansion_search_asmacro(
            &asmacroes, arguments, path, asmacro),
        Vec_free_all(asmacroes, Asmacro_free_for_vec)
    );

    Vec_free_all(asmacroes, Asmacro_free_for_vec);

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_asmoperator(in Asmacro* asmacro, in Vec* arguments, inout Generator* generator) {
    AsmArgs asm_args;

    SRESULT_UNWRAP(
        AsmArgs_from(arguments, &asmacro->arguments, &asm_args),
        (void)NULL
    );
    SRESULT_UNWRAP(
        AsmEncoding_encode(&asmacro->body.asm_operator, &asm_args, generator),
        (void)NULL
    );

    AsmArgs_free(asm_args);
    
    return SResult_new(NULL);
}

static void Syntax_build_asmacro_expansion_useroperator(in Asmacro* asmacro, in Vec* dataes, inout Generator* generator, inout VariableManager* variable_manager) {
    Parser proc_parser = asmacro->body.user_operator;
    
    for(u32 i=0; i<Vec_len(dataes); i++) {
        Argument* arg = Vec_index(&asmacro->arguments, i);
        Data* data = Vec_index(dataes, i);

        Variable variable;
        strcpy(variable.name, arg->name);
        variable.data = Data_clone(data);

        VariableManager_push_alias(variable_manager, variable);
    }

    while(!Parser_is_empty(&proc_parser)) {
        Parser syntax_parser = Parser_split(&proc_parser, ";");
        
        Data data;
        if(!resolve_sresult(
            Syntax_build(syntax_parser, generator, variable_manager, &data),
            syntax_parser.offset, generator
        )) {
            Data_free(data);
        }
    }
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_push_vars_helper(in Vec* arguments, in Data* data, inout VariableManager* variable_manager, inout Generator* generator, bool volatile_flag) {
    Register reg;
    switch(data->storage.type) {
        case StorageType_reg:
            reg = data->storage.body.reg;
            break;
        case StorageType_xmm:
            reg = data->storage.body.xmm;
            break;
        default:
            return SResult_new(NULL);
    }

    if(Vec_len(arguments) != 0) {
        Data* arg = Vec_index(arguments, 0);
        if(arg->storage.type == StorageType_reg && arg->storage.body.reg == reg) {
            return SResult_new(NULL);
        }
        if(arg->storage.type == StorageType_xmm && arg->storage.body.xmm == reg) {
            return SResult_new(NULL);
        }
    }

    if(Register_is_volatile(reg) == volatile_flag) {
        Data arg_data = Data_from_register(reg);
        Vec args = Vec_new(sizeof(Data));
        Vec_push(&args, &arg_data);
        Data data;
        SRESULT_UNWRAP(
            expand_asmacro("push", "", args, generator, variable_manager, &data),
            (void)NULL
        );
        variable_manager->stack_offset -= 8;

        Data_free(data);
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_pop_vars_helper(in Vec* arguments, in Data* data, inout VariableManager* variable_manager, inout Generator* generator, bool volatile_flag) {
    Register reg;
    switch(data->storage.type) {
        case StorageType_reg:
            reg = data->storage.body.reg;
            break;
        case StorageType_xmm:
            reg = data->storage.body.xmm;
            break;
        default:
            return SResult_new(NULL);
    }

    if(Vec_len(arguments) != 0) {
        Data* arg = Vec_index(arguments, 0);
        if(arg->storage.type == StorageType_reg && arg->storage.body.reg == reg) {
            return SResult_new(NULL);
        }
        if(arg->storage.type == StorageType_xmm && arg->storage.body.xmm == reg) {
            return SResult_new(NULL);
        }
    }

    if(Register_is_volatile(reg) == volatile_flag) {
        Data arg_data = Data_from_register(reg);
        Vec args = Vec_new(sizeof(Data));
        Vec_push(&args, &arg_data);
        Data data;
        SRESULT_UNWRAP(
            expand_asmacro("pop", "", args, generator, variable_manager, &data),
            (void)NULL
        );
        variable_manager->stack_offset += 8;

        Data_free(data);
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_push_volatile_vars(in Vec* arguments, inout VariableManager* variable_manager, inout Generator* generator) {
    for(u32 i=0; i<Vec_len(&variable_manager->variables); i++) {
        Variable* variable = Vec_index(&variable_manager->variables, i);
        SRESULT_UNWRAP(
            Syntax_build_asmacro_expansion_fnwrapper_push_vars_helper(arguments, &variable->data, variable_manager, generator, true),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_pop_volatile_vars(in Vec* arguments, inout VariableManager* variable_manager, inout Generator* generator) {
    assert(Vec_size(arguments) == sizeof(Data));

    for(i32 i=Vec_len(&variable_manager->variables)-1; 0<=i; i--) {
        Variable* variable = Vec_index(&variable_manager->variables, i);
        SRESULT_UNWRAP(
            Syntax_build_asmacro_expansion_fnwrapper_pop_vars_helper(arguments, &variable->data, variable_manager, generator, true),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_push_nonvolatile_args(in Vec* arguments, inout VariableManager* variable_manager, inout Generator* generator) {
    for(u32 i=0; i<Vec_len(arguments); i++) {
        Data* fn_arg = Vec_index(arguments, i);
        SRESULT_UNWRAP(
            Syntax_build_asmacro_expansion_fnwrapper_push_vars_helper(arguments, fn_arg, variable_manager, generator, false),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_pop_nonvolatile_args(in Vec* arguments, inout VariableManager* variable_manager, inout Generator* generator) {
    for(i32 i=Vec_len(arguments)-1; 0<=i; i--) {
        Data* fn_arg = Vec_index(arguments, i);
        SRESULT_UNWRAP(
            Syntax_build_asmacro_expansion_fnwrapper_pop_vars_helper(arguments, fn_arg, variable_manager, generator, false),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_push_stack_args(in Asmacro* asmacro, in Vec* arguments, inout VariableManager* variable_manager, inout Generator* generator) {
    assert(asmacro->type == Asmacro_FnWrapper);

    Vec* asmacro_argvars = &asmacro->body.fn_wrapper;
    for(i32 i=(i32)Vec_len(asmacro_argvars)-1; 0<=i; i--) {
        Variable* asmacro_argvar = Vec_index(asmacro_argvars, i);
        Storage* asmacro_argvar_storage = &asmacro_argvar->data.storage;
        if(asmacro_argvar_storage->type != StorageType_mem) {
            continue;
        }

        Type* asmacro_argvar_type = &asmacro_argvar->data.type;
        i32 size = asmacro_argvar_type->size;
        i32 align = asmacro_argvar_type->align;
        Memory* memory = &asmacro_argvar_storage->body.mem;
        assert(memory->base == Rbp);
        assert(memory->disp.label[0] == '\0');
        i32 push_offset = (variable_manager->stack_offset - size - align + 1)/align*align;
        i32 call_stack_offset = (push_offset - memory->disp.offset - 0x0f)/0x10*0x10;
        push_offset = call_stack_offset + memory->disp.offset;

        SRESULT_UNWRAP(
            sub_rsp(variable_manager->stack_offset-push_offset, generator, variable_manager),
            (void)NULL
        );

        Vec mov_args = Vec_new(sizeof(Data));
        Data* mov_arg_src_ptr = Vec_index(arguments, i);
        Data mov_arg_dst = Data_from_mem(Rbp, push_offset, Type_clone(&mov_arg_src_ptr->type));
        Data mov_arg_src = Data_clone(mov_arg_src_ptr);
        Vec_push(&mov_args, &mov_arg_dst);
        Vec_push(&mov_args, &mov_arg_src);
        Data data;
        SRESULT_UNWRAP(
            expand_asmacro("mov", "", mov_args, generator, variable_manager, &data),
            (void)NULL
        );
        Data_free(data);
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper(in Asmacro* asmacro, in Vec* arguments, inout Generator* generator, inout VariableManager* variable_manager) {
    i32 stack_offset = variable_manager->stack_offset;

    SRESULT_UNWRAP(
        Syntax_build_asmacro_expansion_fnwrapper_push_volatile_vars(arguments, variable_manager, generator),
        (void)NULL
    );
    SRESULT_UNWRAP(
        Syntax_build_asmacro_expansion_fnwrapper_push_nonvolatile_args(arguments, variable_manager, generator),
        (void)NULL
    );
    i32 stack_offset_before_push = variable_manager->stack_offset;
    SRESULT_UNWRAP(
        Syntax_build_asmacro_expansion_fnwrapper_push_stack_args(asmacro, arguments, variable_manager, generator),
        (void)NULL
    );

    Vec call_args = Vec_new(sizeof(Data));
    Data call_label = Data_from_label(asmacro->name);
    Vec_push(&call_args, &call_label);
    Data data;
    SRESULT_UNWRAP(
        expand_asmacro("call", "", call_args, generator, variable_manager, &data),
        (void)NULL
    );
    Data_free(data);

    SRESULT_UNWRAP(
        sub_rsp(variable_manager->stack_offset - stack_offset_before_push, generator, variable_manager),
        (void)NULL
    );
    assert(variable_manager->stack_offset == stack_offset_before_push);
    SRESULT_UNWRAP(
        Syntax_build_asmacro_expansion_fnwrapper_pop_nonvolatile_args(arguments, variable_manager, generator),
        (void)NULL
    );
    SRESULT_UNWRAP(
        Syntax_build_asmacro_expansion_fnwrapper_pop_volatile_vars(arguments, variable_manager, generator),
        (void)NULL
    );

    variable_manager->stack_offset = stack_offset;

    return SResult_new(NULL);
}

SResult expand_asmacro(in char* name, in char* path, Vec arguments, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(Vec_size(&arguments) == sizeof(Data));
    Asmacro asmacro;

    SRESULT_UNWRAP(
        Syntax_build_asmacro_expansion_get_info(
            name,
            path,
            generator,
            &arguments,
            &asmacro),
        Vec_free_all(arguments, Data_free_for_vec)
    );

    SRESULT_UNWRAP(
        Generator_asmacro_expand_check(generator, Asmacro_clone(&asmacro)),
        Vec_free_all(arguments, Data_free_for_vec);
        Asmacro_free(asmacro);
    );

    VariableManager_new_block(variable_manager);

    switch(asmacro.type) {
        case Asmacro_AsmOperator:
            SRESULT_UNWRAP(
                Syntax_build_asmacro_expansion_asmoperator(&asmacro, &arguments, generator),
                Asmacro_free(asmacro);Vec_free_all(arguments, Data_free_for_vec);
            );
            break;
        case Asmacro_UserOperator:
            Syntax_build_asmacro_expansion_useroperator(&asmacro, &arguments, generator, variable_manager);
            break;
        case Asmacro_FnWrapper:
            SRESULT_UNWRAP(
                Syntax_build_asmacro_expansion_fnwrapper(&asmacro, &arguments, generator, variable_manager),
                Asmacro_free(asmacro);Vec_free_all(arguments, Data_free_for_vec)
            );
            break;
    }

    if(Vec_len(&arguments) != 0) {
        *data = Data_clone(Vec_index(&arguments, 0));
    }else {
        *data = Data_void();
    }
    Asmacro_free(asmacro);
    Vec_free_all(arguments, Data_free_for_vec);

    SRESULT_UNWRAP(
        VariableManager_delete_block(variable_manager, generator),
        (void)NULL
    );

    Generator_finish_asmacro_expand(generator);

    return SResult_new(NULL);
}

static SResult sub_rsp(i32 value, inout Generator* generator, inout VariableManager* variable_manager) {
    if(value != 0) {
        Vec args = Vec_new(sizeof(Data));
        Data rsp = Data_from_register(Rsp);
        Data imm = Data_from_imm(value);
        Vec_push(&args, &rsp);
        Vec_push(&args, &imm);
        Data data;

        SRESULT_UNWRAP(
            expand_asmacro("sub", "", args, generator, variable_manager, &data),
            (void)NULL
        );
        variable_manager->stack_offset -= value;
        Data_free(data);
    }

    return SResult_new(NULL);
}

bool Syntax_build_asmacro_expansion(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    char name[256];
    if(!ParserMsg_is_success(Parser_parse_ident(&parser, name))) {
        return false;
    }
    Parser args_parser;
    if(!ParserMsg_is_success(Parser_parse_paren(&parser, &args_parser))) {
        return false;
    }

    *data = Data_void();
    Vec arguments;

    if(resolve_sresult(
        Syntax_build_asmacro_expansion_get_arguments(args_parser, generator, variable_manager, &arguments),
        parser.offset, generator
    )) {
        return true;
    }
    
    if(resolve_sresult(
        expand_asmacro(name, Parser_path(&parser), arguments, generator, variable_manager, data),
        parser.offset, generator
    )) {
        *data = Data_void();
        return true;
    }
    
    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_variable_declaration(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "let"))) {
        return false;
    }
    
    *data = Data_void();
    
    Variable variable;
    i32 stack_offset = variable_manager->stack_offset;
    if(resolve_parsermsg(Variable_parse(&parser, generator, &variable_manager->stack_offset, &variable), generator)) {
        return true;
    }

    Vec sub_args = Vec_new(sizeof(Data));
    Data sub_arg_rsp = Data_from_register(Rsp);
    Data sub_arg_imm = Data_from_imm(stack_offset - variable_manager->stack_offset);
    Vec_push(&sub_args, &sub_arg_rsp);
    Vec_push(&sub_args, &sub_arg_imm);
    Data sub_data;
    if(!resolve_sresult(expand_asmacro("sub", "", sub_args, generator, variable_manager, &sub_data), parser.offset, generator)) {
        Data_free(sub_data);
    }

    resolve_sresult(
        VariableManager_push(variable_manager, variable, generator),
        parser.offset,
        generator
    );
    
    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_register_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    (void)generator;(void)variable_manager;
    Register reg;
    
    if(!ParserMsg_is_success(Register_parse(&parser, &reg))) {
        return false;
    }
    
    *data = Data_from_register(reg);
    
    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_imm_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    (void)variable_manager;
    u64 imm;
    if(!ParserMsg_is_success(Parser_parse_number(&parser, &imm))) {
        return false;
    }
    
    *data = Data_from_imm(imm);
    
    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_true_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(generator != NULL && variable_manager != NULL && data != NULL);

    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "true"))) {
        return false;
    }

    *data = Data_true();

    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_false_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(generator != NULL && variable_manager != NULL && data != NULL);

    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "false"))) {
        return false;
    }

    *data = Data_false();

    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_null_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(generator != NULL && variable_manager != NULL && data != NULL);

    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "null"))) {
        return false;
    }

    *data = Data_null();

    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_char_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(generator != NULL && variable_manager != NULL && data != NULL);

    if(!Parser_start_with_symbol(&parser, "\'")) {
        return false;
    }

    char code = '\0';
    if(resolve_parsermsg(
        Parser_parse_char(&parser, &code), generator
    )) {
        *data = Data_void();
        return true;
    }

    *data = Data_from_char(code);

    return true;
}

bool Syntax_build_assignment(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Parser left_parser = Parser_split(&parser, "=");
    Parser right_parser = parser;
    if(Parser_is_empty(&right_parser)) {
        return false;
    }
    *data = Data_void();
    Data left_data;
    Data right_data;

    if(resolve_sresult(Syntax_build(right_parser, generator, variable_manager, &right_data), parser.offset, generator)) {
        return true;
    }
    if(resolve_sresult(Syntax_build(left_parser, generator, variable_manager, &left_data), parser.offset, generator)) {
        Data_free(right_data);
        return true;
    }

    Vec mov_args = Vec_new(sizeof(Data));
    Vec_push(&mov_args, &left_data);
    Vec_push(&mov_args, &right_data);
    if(resolve_sresult(expand_asmacro("mov", Parser_path(&parser), mov_args, generator, variable_manager, data), parser.offset, generator)) {
        *data = Data_void();
    }

    return true;
}

bool Syntax_build_dot_operator(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(generator != NULL && variable_manager != NULL && data != NULL);

    Parser left = Parser_rsplit(&parser, ".");
    if(Parser_is_empty(&left) || Parser_is_empty(&parser)) {
        return false;
    }

    *data = Data_void();

    Data left_data;
    if(resolve_sresult(
        Syntax_build(left, generator, variable_manager, &left_data), left.offset, generator
    )) {
        return true;
    }
    char element[256];
    if(resolve_parsermsg(
        Parser_parse_ident(&parser, element), generator
    )) {
        Data_free(left_data);
        return true;
    }
    check_parser(&parser, generator);

    if(resolve_sresult(
        Data_dot_operator(&left_data, element, data), parser.offset, generator
    )) {
        Data_free(left_data);
        *data = Data_void();
        return true;
    }
    
    Data_free(left_data);

    return true;
}

bool Syntax_build_refer_operator(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(generator != NULL && variable_manager != NULL && data != NULL);
    if(!ParserMsg_is_success(Parser_parse_symbol(&parser, "*"))) {
        return false;
    }

    *data = Data_void();

    Data ptr_data;
    if(resolve_sresult(
        Syntax_build(parser, generator, variable_manager, &ptr_data), parser.offset, generator
    )) {
        return true;
    }

    if(resolve_sresult(
        Data_ref(ptr_data, generator, data), parser.offset, generator
    )) {
        *data = Data_void();
        return true;
    }

    return true;
}

bool Syntax_build_subscript_operator(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(generator != NULL && variable_manager != NULL && data != NULL);

    Parser left_parser = Parser_split(&parser, "[");
    if(Parser_is_empty(&left_parser) || Parser_is_empty(&parser)) {
        return false;
    }

    *data = Data_void();

    Parser index_parser = Parser_split(&parser, "]");
    if(Parser_is_empty(&index_parser)) {
        Generator_add_error(generator, Error_new(parser.offset, "expected index"));
        return true;
    }

    Data left_data;
    if(resolve_sresult(
        Syntax_build(left_parser, generator, variable_manager, &left_data),
        parser.offset, generator
    )) {
        return true;
    }

    Data index_data;
    if(resolve_sresult(
        Syntax_build(index_parser, generator, variable_manager, &index_data),
        parser.offset, generator
    )) {
        Data_free(left_data);
        return true;
    }

    if(resolve_sresult(
        Data_subscript(left_data, index_data, data),
        parser.offset, generator
    )) {
        *data = Data_void();
    }

    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_variable_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    char name[256];
    if(!ParserMsg_is_success(Parser_parse_ident(&parser, name))) {
        return false;
    }
    
    *data = Data_void();
    
    Variable variable;
    if(resolve_sresult(VariableManager_get(variable_manager, name, &variable), parser.offset, generator)) {
        return true;
    }
    
    *data = variable.data;
    
    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_sizeof_operator(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(generator != NULL && variable_manager != NULL && data != NULL);

    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "sizeof"))) {
        return false;
    }

    u32 size = 0;

    Data expr_data;
    Type type;
    if(ParserMsg_is_success(
        Type_parse(&parser, generator, &type)
    )) {
        size = type.size;
        Type_free(type);
        check_parser(&parser, generator);
    }else if(!resolve_sresult(
        Syntax_build(parser, generator, variable_manager, &expr_data), parser.offset, generator
    )) {
        size = expr_data.type.size;
        Data_free(expr_data);
    }else {
        *data = Data_void();
        return true;
    }

    *data = Data_from_imm(size);

    return true;
}

bool Syntax_build_alignof_operator(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(generator != NULL && variable_manager != NULL && data != NULL);

    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "alignof"))) {
        return false;
    }

    u32 align = 1;

    Data expr_data;
    Type type;
    if(ParserMsg_is_success(
        Type_parse(&parser, generator, &type)
    )) {
        align = type.align;
        Type_free(type);
        check_parser(&parser, generator);
    }else if(!resolve_sresult(
        Syntax_build(parser, generator, variable_manager, &expr_data), parser.offset, generator
    )) {
        align = expr_data.type.align;
        Data_free(expr_data);
    }else {
        *data = Data_void();
        return true;
    }
    
    *data = Data_from_imm(align);

    return true;
}

bool Syntax_build_return(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "return"))) {
        return false;
    }

    Vec ret_arg = Vec_new(sizeof(Data));
    Data ret_rel = Data_from_label(".ret");
    Vec_push(&ret_arg, &ret_rel);
    if(!resolve_sresult(expand_asmacro("jmp", Parser_path(&parser), ret_arg, generator, variable_manager, data), parser.offset, generator)) {
        Data_free(*data);
    }
    *data = Data_void();

    check_parser(&parser, generator);
    return true;
}



