#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "types.h"
#include "gen.h"
#include "syntax.h"
#include "util.h"

static SResult sub_rsp_raw(i32 value, inout Generator* generator, inout VariableManager* variable_manager);
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
        if(Storage_cmp(&variable->data.storage, &ptr->data.storage)
            || Storage_is_depend_on(&variable->data.storage, &ptr->data.storage)) {
            return true;
        }
    }

    return false;
}

static SResult drop_variable(in Variable* variable, inout VariableManager* variable_manager, inout Generator* generator) {
    Vec drop_args = Vec_new(sizeof(Data));
    Data drop_data = Data_clone(&variable->data);
    Vec_push(&drop_args, &drop_data);

    Data data;
    SRESULT_UNWRAP(
        expand_asmacro("drop", "", drop_args, generator, variable_manager, &data), (void)NULL
    );
    Data_free(data);

    return SResult_new(NULL);
}

SResult VariableManager_push(inout VariableManager* self, Variable variable, inout Generator* generator) {
    u32 variable_base = get_last_block(self)->variables_base;
    
    for(i32 i=Vec_len(&self->variables)-1; 0<=i; i--) {
        Variable* ptr = Vec_index(&self->variables, i);
        
        bool doubling_storage_flag = Storage_cmp(&ptr->data.storage, &variable.data.storage);

        if(doubling_storage_flag) {
            if((i32)variable_base <= i) {
                if(ptr->defer_flag) {
                    SRESULT_UNWRAP(
                        drop_variable(ptr, self, generator), (void)NULL
                    );
                    ptr = Vec_index(&self->variables, i);
                }
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
    variable.defer_flag = false;
    Vec_push(&self->variables, &variable);
}

SResult VariableManager_get(inout VariableManager* self, in char* name, out Variable* variable) {
    for(i32 i=Vec_len(&self->variables)-1; 0<=i; i--) {
        Variable* ptr = Vec_index(&self->variables, i);
        if(strcmp(ptr->name, name) == 0) {
            if(is_stored(self, ptr, i)) {
                char msg[256];
                wrapped_strcpy(msg, "variable \"", sizeof(msg));
                wrapped_strcat(msg, name, sizeof(msg));
                wrapped_strcat(msg, "\" is shadowed", sizeof(msg));
                return SResult_new(msg);
            }
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
    Data mov_src = Data_from_mem(Rbp, stored_reg->stack_offset, NULL, type);
    Data mov_dst = Data_from_register(stored_reg->reg);
    Vec_push(&mov_args, &mov_dst);
    Vec_push(&mov_args, &mov_src);

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

    VariableBlock* block_ptr = get_last_block(self);
    u32 variables_base = block_ptr->variables_base;
    for(i32 i=Vec_len(&self->variables)-1; (i32)(variables_base)<=i; i--) {
        Variable* variable = Vec_index(&self->variables, i);
        if(variable->defer_flag && variable->name[0] != '\0') {
            SRESULT_UNWRAP(
                drop_variable(variable, self, generator), (void)NULL
            );
        }
    }

    VariableBlock block;
    Vec_pop(&self->blocks, &block);
    for(i32 i=Vec_len(&self->variables)-1; (i32)(block.variables_base)<=i; i--) {
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

SResult VariableManager_delete_allblock(inout VariableManager* self, inout Generator* generator) {
    for(i32 i=Vec_len(&self->blocks)-1; 0<=i; i--) {
        SRESULT_UNWRAP(
            VariableManager_delete_block(self, generator), (void)NULL
        );
    }

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

void SyntaxTree_build(Parser parser, inout Generator* generator, inout VariableManager* variable_manager) {
    VariableManager_new_block(variable_manager);

    while(!Parser_is_empty(&parser)) {
        Parser syntax_parser = Parser_split(&parser, ";");
        Data data;
        if(resolve_sresult(Syntax_build(syntax_parser, generator, variable_manager, &data), syntax_parser.offset, generator)) {
            return;
        }
        Data_free(data);
    }

    resolve_sresult(
        VariableManager_delete_block(variable_manager, generator), parser.offset, generator
    );

    return;
}

SResult Syntax_build(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    bool (*BUILDERS[])(Parser, inout Generator*, inout VariableManager* variable_manager, out Data*) = {
        Syntax_build_variable_declaration,
        Syntax_build_static_variable_declaration,
        Syntax_build_const_variable_declaration,
        Syntax_build_return,
        Syntax_build_sizeof_operator,
        Syntax_build_alignof_operator,
        Syntax_build_if,
        Syntax_build_for,
        Syntax_build_while,
        Syntax_build_asmacro_expansion,
        Syntax_build_call_fnptr,
        Syntax_build_block,
        Syntax_build_paren,
        Syntax_build_register_expression,
        Syntax_build_imm_expression,
        Syntax_build_true_expression,
        Syntax_build_false_expression,
        Syntax_build_null_expression,
        Syntax_build_char_expression,
        Syntax_build_implicit_static_string,
        Syntax_build_debug_label_syntax,
        Syntax_build_assignadd,
        Syntax_build_assignsub,
        Syntax_build_assignand,
        Syntax_build_assignor,
        Syntax_build_assignxor,
        Syntax_build_assignlea,
        Syntax_build_assignment,
        Syntax_build_enum_expr,
        Syntax_build_inc,
        Syntax_build_dec,
        Syntax_build_dot_operator,
        Syntax_build_refer_operator,
        Syntax_build_subscript_operator,
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

static bool Syntax_build_asmacro_expansion_search_asmacro(in Vec* asmacroes, in Vec* arguments, in char* path, out Asmacro* asmacro) {
    assert(asmacroes != NULL);
    assert(arguments != NULL);
    assert(path != NULL);
    assert(asmacro != NULL);

    for(u32 i=0; i<Vec_len(asmacroes); i++) {
        Asmacro* ptr = Vec_index(asmacroes, i);
        if(SResult_is_success(Asmacro_match_with(ptr, arguments, path))) {
            *asmacro = Asmacro_clone(ptr);
            return true;
        }
    }

    return false;
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

    if(!Syntax_build_asmacro_expansion_search_asmacro(&asmacroes, arguments, path, asmacro)) {
        Vec_free_all(asmacroes, Asmacro_free_for_vec);
        char msg[256];
        snprintf(msg, 256, "mismatching asmacro for \"%.229s\"", name);
        return SResult_new(msg);
    }

    Vec_free_all(asmacroes, Asmacro_free_for_vec);

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_asmmacro(in Asmacro* asmacro, in Vec* arguments, inout Generator* generator) {
    AsmArgs asm_args;

    SRESULT_UNWRAP(
        AsmArgs_from(arguments, &asmacro->arguments, &asm_args),
        (void)NULL
    );
    SRESULT_UNWRAP(
        AsmEncoding_encode(&asmacro->body.asm_macro, &asm_args, generator),
        (void)NULL
    );

    AsmArgs_free(asm_args);
    
    return SResult_new(NULL);
}

static void Syntax_build_asmacro_expansion_usermacro(in Asmacro* asmacro, in Vec* dataes, inout Generator* generator, inout VariableManager* variable_manager) {
    Parser proc_parser = asmacro->body.user_macro;

    for(u32 i=0; i<Vec_len(dataes); i++) {
        Argument* arg = Vec_index(&asmacro->arguments, i);
        Data* data = Vec_index(dataes, i);

        Variable variable = Variable_new(arg->name, "", Data_clone(data), false);

        VariableManager_push_alias(variable_manager, variable);
    }

    VariableManager_new_block(variable_manager);

    while(!Parser_is_empty(&proc_parser)) {
        Parser syntax_parser = Parser_split(&proc_parser, ";");
        
        Data data;
        if(resolve_sresult(
            Syntax_build(syntax_parser, generator, variable_manager, &data),
            syntax_parser.offset, generator
        )) {
            return;
        }
        Data_free(data);
    }

    resolve_sresult(
        VariableManager_delete_block(variable_manager, generator), proc_parser.offset, generator
    );
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_push_vars_helper(in Vec* arguments, in Variable* variable, u32 variable_index, inout VariableManager* variable_manager, inout Generator* generator, bool volatile_flag) {
    Register reg;
    switch(variable->data.storage.type) {
        case StorageType_reg:
            reg = variable->data.storage.body.reg;
            break;
        case StorageType_xmm:
            reg = variable->data.storage.body.xmm;
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

    if(Register_is_volatile(reg) == volatile_flag && !is_stored(variable_manager, variable, variable_index)) {
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

static SResult Syntax_build_asmacro_expansion_fnwrapper_pop_vars_helper(in Vec* arguments, in Variable* variable, u32 variable_index, inout VariableManager* variable_manager, inout Generator* generator, bool volatile_flag) {
    Register reg;
    switch(variable->data.storage.type) {
        case StorageType_reg:
            reg = variable->data.storage.body.reg;
            break;
        case StorageType_xmm:
            reg = variable->data.storage.body.xmm;
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

    if(Register_is_volatile(reg) == volatile_flag && !is_stored(variable_manager, variable, variable_index)) {
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

static SResult Syntax_build_asmacro_expansion_fnwrapper_push_args_helper(in Vec* arguments, in Data* data, inout VariableManager* variable_manager, inout Generator* generator, bool volatile_flag) {
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

static SResult Syntax_build_asmacro_expansion_fnwrapper_pop_args_helper(in Vec* arguments, in Data* data, inout VariableManager* variable_manager, inout Generator* generator, bool volatile_flag) {
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
            Syntax_build_asmacro_expansion_fnwrapper_push_vars_helper(arguments, variable, i, variable_manager, generator, true),
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
            Syntax_build_asmacro_expansion_fnwrapper_pop_vars_helper(arguments, variable, i, variable_manager, generator, true),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_push_nonvolatile_args(in Vec* arguments, inout VariableManager* variable_manager, inout Generator* generator) {
    for(u32 i=0; i<Vec_len(arguments); i++) {
        Data* fn_arg = Vec_index(arguments, i);
        SRESULT_UNWRAP(
            Syntax_build_asmacro_expansion_fnwrapper_push_args_helper(arguments, fn_arg, variable_manager, generator, false),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_pop_nonvolatile_args(in Vec* arguments, inout VariableManager* variable_manager, inout Generator* generator) {
    for(i32 i=Vec_len(arguments)-1; 0<=i; i--) {
        Data* fn_arg = Vec_index(arguments, i);
        SRESULT_UNWRAP(
            Syntax_build_asmacro_expansion_fnwrapper_pop_args_helper(arguments, fn_arg, variable_manager, generator, false),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_push_stack_args(in Asmacro* asmacro, in Vec* arguments, inout VariableManager* variable_manager, inout Generator* generator) {
    Vec* asmacro_argvars = NULL;
    switch(asmacro->type) {
        case Asmacro_FnWrapper:
            asmacro_argvars = &asmacro->body.fn_wrapper;
            break;
        case Asmacro_FnExtern:
            asmacro_argvars = &asmacro->body.fn_extern;
            break;
        default: PANIC("unreachable");
    }
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
        Data mov_arg_dst = Data_from_mem(Rbp, push_offset, NULL, Type_clone(&mov_arg_src_ptr->type));
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

static SResult Syntax_build_asmacro_expansion_fnwrapper(in Asmacro* asmacro, in Data call_to, in Vec* arguments, inout Generator* generator, inout VariableManager* variable_manager) {
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
    Vec_push(&call_args, &call_to);
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
        case Asmacro_AsmMacro:
            SRESULT_UNWRAP(
                Syntax_build_asmacro_expansion_asmmacro(&asmacro, &arguments, generator),
                Asmacro_free(asmacro);Vec_free_all(arguments, Data_free_for_vec);
            );
            break;
        case Asmacro_UserMacro:
            Syntax_build_asmacro_expansion_usermacro(&asmacro, &arguments, generator, variable_manager);
            break;
        case Asmacro_FnWrapper:
        case Asmacro_FnExtern:
        {
            Data call_to = Data_from_label(asmacro.name);
            SRESULT_UNWRAP(
                Syntax_build_asmacro_expansion_fnwrapper(&asmacro, call_to, &arguments, generator, variable_manager),
                Asmacro_free(asmacro);Vec_free_all(arguments, Data_free_for_vec)
            );
        }
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
        Data_free(*data)
    );
    
    Generator_finish_asmacro_expand(generator);

    return SResult_new(NULL);
}

static SResult sub_rsp_raw(i32 value, inout Generator* generator, inout VariableManager* variable_manager) {
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
        Data_free(data);
    }

    return SResult_new(NULL);
}

static SResult sub_rsp(i32 value, inout Generator* generator, inout VariableManager* variable_manager) {
    SRESULT_UNWRAP(
        sub_rsp_raw(value, generator, variable_manager), (void)NULL
    );

    variable_manager->stack_offset -= value;
    return SResult_new(NULL);
}

bool Syntax_build_asmacro_expansion(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Parser firstarg_parser = Parser_rsplit(&parser, ".");
    if(Parser_is_empty(&parser)) {
        parser = firstarg_parser;
        firstarg_parser = Parser_empty(parser.offset);
    }

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

    if(!Parser_is_empty(&firstarg_parser)) {
        Data firstarg_data;
        if(resolve_sresult(
            Syntax_build(firstarg_parser, generator, variable_manager, &firstarg_data), firstarg_parser.offset, generator
        )) {
            Vec_free_all(arguments, Data_free_for_vec);
            return true;
        }
        Vec_insert(&arguments, 0, &firstarg_data);
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

static ParserMsg Syntax_build_call_fnptr_parse(Parser parser, inout Generator* generator, out Parser* fnptr_parser, out Parser* arg_parser) {
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser, fnptr_parser), (void)NULL
    );
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser, arg_parser), (void)NULL
    );

    check_parser(&parser, generator);

    return ParserMsg_new(parser.offset, NULL);
}

static SResult Syntax_build_call_fnptr_build(Parser fnptr_parser, Parser args_parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Vec arguments;
    SRESULT_UNWRAP(
        Syntax_build_asmacro_expansion_get_arguments(args_parser, generator, variable_manager, &arguments),
        (void)NULL
    );

    Data fnptr_data;
    SRESULT_UNWRAP(
        Syntax_build(fnptr_parser, generator, variable_manager, &fnptr_data),
        Vec_free_all(arguments, Data_free_for_vec);
    );

    Type fn_type;
    SRESULT_UNWRAP(
        Type_ref(&fnptr_data.type, generator, &fn_type),
        Vec_free_all(arguments, Data_free_for_vec);
        Data_free(fnptr_data);
    );
    if(fn_type.type != Type_Fn) {
        Vec_free_all(arguments, Data_free_for_vec);
        Data_free(fnptr_data);
        Type_free(fn_type);
        return SResult_new("expected fn ptr");
    }
    
    SResult result;
    Vec var_args = Variables_from_datas(&fn_type.body.t_fn);
    Asmacro asmacro = Asmacro_new_fn_wrapper("", var_args, "");
    if(SResult_is_success(Asmacro_match_with(&asmacro, &arguments, ""))) {
        VariableManager_new_block(variable_manager);
        result = Syntax_build_asmacro_expansion_fnwrapper(&asmacro, fnptr_data, &arguments, generator, variable_manager);
        SRESULT_UNWRAP(
            VariableManager_delete_block(variable_manager, generator),
            Asmacro_free(asmacro);Type_free(fn_type);Vec_free_all(arguments, Data_free_for_vec)
        );
        if(SResult_is_success(result)) {
            if(Vec_len(&arguments) != 0) {
                *data = Data_clone(Vec_index(&arguments, 0));
            }else {
                *data = Data_void();
            }
        }
    }else {
        result = SResult_new("mismatching arguments");
    }
    Asmacro_free(asmacro);
    Type_free(fn_type);
    Vec_free_all(arguments, Data_free_for_vec);

    return result;
}

bool Syntax_build_call_fnptr(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Parser fnptr_parser;
    Parser args_parser;
    if(!ParserMsg_is_success(Syntax_build_call_fnptr_parse(parser, generator, &fnptr_parser, &args_parser))) {
        return false;
    }

    *data = Data_void();
    if(Parser_is_empty(&fnptr_parser)) {
        Error error = Error_new(parser.offset, "expected fnptr");
        Generator_add_error(generator, error);
        return true;
    }

    resolve_sresult(
        Syntax_build_call_fnptr_build(fnptr_parser, args_parser, generator, variable_manager, data),
        parser.offset, generator
    );

    return true;
}


static SResult build_assignops_data(in char* opname, Data left_data, Data right_data, inout Generator* generator, inout VariableManager* variable_manager) {
    Vec op_args = Vec_new(sizeof(Data));
    Vec_push(&op_args, &left_data);
    Vec_push(&op_args, &right_data);
    Data data;
    SRESULT_UNWRAP(
        expand_asmacro(opname, "", op_args, generator, variable_manager, &data), (void)NULL
    );
    Data_free(data);

    return SResult_new(NULL);
}

static SResult build_assignops(in char* opname, Parser left_parser, Parser right_parser, inout Generator* generator, inout VariableManager* variable_manager) {
    Data right_data;
    Data left_data;

    SRESULT_UNWRAP(
        Syntax_build(right_parser, generator, variable_manager, &right_data), (void)NULL
    );

    SRESULT_UNWRAP(
        Syntax_build(left_parser, generator, variable_manager, &left_data), (void)NULL
    );

    SRESULT_UNWRAP(
        build_assignops_data(opname, left_data, right_data, generator, variable_manager), (void)NULL
    );
    
    return SResult_new(NULL);
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

    resolve_sresult(
        sub_rsp_raw(stack_offset - variable_manager->stack_offset, generator, variable_manager), parser.offset, generator
    );
 
    resolve_sresult(
        VariableManager_push(variable_manager, variable, generator),
        parser.offset,
        generator
    );

    if(ParserMsg_is_success(Parser_parse_symbol(&parser, "="))) {
        Data init_data;
        if(!resolve_sresult(Syntax_build(parser, generator, variable_manager, &init_data), parser.offset, generator)) {
            resolve_sresult(
                build_assignops_data("mov", Data_clone(&variable.data), init_data, generator, variable_manager), parser.offset, generator
            );
        }
    }else {
        check_parser(&parser, generator);
    }
    
    return true;
}

bool Syntax_build_static_variable_declaration(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "static"))) {
        return false;
    }

    *data = Data_void();

    Variable variable;
    if(resolve_parsermsg(
        Variable_parse_static_local(&parser, generator, &variable), generator
    )) {
        return true;
    }
    resolve_sresult(
        VariableManager_push(variable_manager, variable, generator), parser.offset, generator
    );

    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_const_variable_declaration(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "const"))) {
        return false;
    }

    *data = Data_void();

    Variable variable;
    if(resolve_parsermsg(
        Variable_parse_const_local(&parser, generator, &variable), generator
    )) {
        return true;
    }
    resolve_sresult(
        VariableManager_push(variable_manager, variable, generator), parser.offset, generator
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

    resolve_sresult(
        build_assignops("mov", left_parser, right_parser, generator, variable_manager), parser.offset, generator
    );

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
        Data_subscript(left_data, index_data, generator, data),
        parser.offset, generator
    )) {
        *data = Data_void();
    }

    check_parser(&parser, generator);
    return true;
}

bool Syntax_build_enum_expr(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(generator != NULL && variable_manager != NULL && data != NULL);

    Type enum_type;
    if(!ParserMsg_is_success(Type_parse(&parser, generator, &enum_type))) {
        return false;
    }

    if(enum_type.type != Type_Enum) {
        Type_free(enum_type);
        return false;
    }

    *data = Data_void();

    if(resolve_parsermsg(
        Parser_parse_symbol(&parser, "."), generator
    )) {
        Type_free(enum_type);
        return true;
    }

    if(resolve_parsermsg(
        Type_get_enum_member(enum_type, parser, data), generator
    )) {
        *data = Data_void();
    }

    return true;
}

bool Syntax_build_variable_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    char name[256];
    if(!ParserMsg_is_success(Parser_parse_ident(&parser, name))) {
        return false;
    }
    
    *data = Data_void();
    
    Variable variable;
    if(!SResult_is_success(Generator_get_global_variable(generator, Parser_path(&parser), name, &variable))
        && resolve_sresult(VariableManager_get(variable_manager, name, &variable), parser.offset, generator)) {
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

    Parser paren_parser;
    if(resolve_parsermsg(Parser_parse_paren(&parser, &paren_parser), generator)) {
        *data = Data_void();
        return true;
    }

    u32 size = 0;

    Data expr_data;
    Type type;
    if(ParserMsg_is_success(
        Type_parse(&paren_parser, generator, &type)
    )) {
        size = type.size;
        Type_free(type);
        check_parser(&paren_parser, generator);
    }else if(!resolve_sresult(
        Syntax_build(paren_parser, generator, variable_manager, &expr_data), parser.offset, generator
    )) {
        size = expr_data.type.size;
        Data_free(expr_data);
    }else {
        *data = Data_void();
        return true;
    }
    check_parser(&parser, generator);

    *data = Data_from_imm(size);

    return true;
}

bool Syntax_build_alignof_operator(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    assert(generator != NULL && variable_manager != NULL && data != NULL);

    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "alignof"))) {
        return false;
    }

    Parser paren_parser;
    if(resolve_parsermsg(Parser_parse_paren(&parser, &paren_parser), generator)) {
        *data = Data_void();
        return true;
    }

    u32 align = 1;

    Data expr_data;
    Type type;
    if(ParserMsg_is_success(
        Type_parse(&paren_parser, generator, &type)
    )) {
        align = type.align;
        Type_free(type);
        check_parser(&paren_parser, generator);
    }else if(!resolve_sresult(
        Syntax_build(paren_parser, generator, variable_manager, &expr_data), parser.offset, generator
    )) {
        align = expr_data.type.align;
        Data_free(expr_data);
    }else {
        *data = Data_void();
        return true;
    }
    check_parser(&parser, generator);
    
    *data = Data_from_imm(align);

    return true;
}

static ParserMsg Syntax_build_if_parse(Parser parser, inout Generator* generator, out Parser* condition, out Parser* then_branch, out Parser* else_branch) {
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser, condition), (void)NULL
    );
    PARSERMSG_UNWRAP(
        Parser_parse_block(&parser, then_branch), (void)NULL
    );
    if(ParserMsg_is_success(Parser_parse_keyword(&parser, "else"))) {
        PARSERMSG_UNWRAP(
            Parser_parse_block(&parser, else_branch), (void)NULL
        );
    }else {
        *else_branch = Parser_empty(parser.offset);
    }

    check_parser(&parser, generator);

    return ParserMsg_new(parser.offset, NULL);
}

static SResult build_cmpzero(Data data, inout Generator* generator, inout VariableManager* variable_manager) {
    Data tmp_data;

    Vec cmp_args = Vec_new(sizeof(Data));
    Data cmp_arg2 = Data_from_imm(0);
    Vec_push(&cmp_args, &data);
    Vec_push(&cmp_args, &cmp_arg2);

    SRESULT_UNWRAP(
        expand_asmacro("cmp", "", cmp_args, generator, variable_manager, &tmp_data), (void)NULL
    );
    
    Data_free(tmp_data);

    return SResult_new(NULL);
}

static SResult build_jne(in char* label, inout Generator* generator, inout VariableManager* variable_manager) {
    Data tmp_data;

    Vec jne_args = Vec_new(sizeof(Data));
    Data jne_arg1 = Data_from_label(label);
    Vec_push(&jne_args, &jne_arg1);

    SRESULT_UNWRAP(
        expand_asmacro("jne", "", jne_args, generator, variable_manager, &tmp_data), (void)NULL
    );

    Data_free(tmp_data);

    return SResult_new(NULL);
}

static SResult build_je(in char* label, inout Generator* generator, inout VariableManager* variable_manager) {
    Data tmp_data;

    Vec jne_args = Vec_new(sizeof(Data));
    Data jne_arg1 = Data_from_label(label);
    Vec_push(&jne_args, &jne_arg1);

    SRESULT_UNWRAP(
        expand_asmacro("je", "", jne_args, generator, variable_manager, &tmp_data), (void)NULL
    );

    Data_free(tmp_data);

    return SResult_new(NULL);
}

static SResult build_je_to_else(u32 id, inout Generator* generator, inout VariableManager* variable_manager) {
    char label[256];
    snprintf(label, 256, ".%u.if.else", id);

    SRESULT_UNWRAP(
        build_je(label, generator, variable_manager), (void)NULL
    );

    return SResult_new(NULL);
}

static SResult build_branch(Parser parser, u32 id, in char* branch_name, inout Generator* generator, inout VariableManager* variable_manager) {
    char branch_label[256];
    snprintf(branch_label, 256, ".%u.if.", id);
    wrapped_strcat(branch_label, branch_name, sizeof(branch_label));
    SRESULT_UNWRAP(
        Generator_append_label(generator, ".text", branch_label, false, Label_Notype), (void)NULL
    );

    VariableManager_new_block(variable_manager);

    SyntaxTree_build(parser, generator, variable_manager);

    SRESULT_UNWRAP(
        VariableManager_delete_block(variable_manager, generator), (void)NULL
    );

    return SResult_new(NULL);
}

static SResult build_jmp(in char* label, inout Generator* generator, inout VariableManager* variable_manager) {
    Data tmp_data;

    Vec jmp_args = Vec_new(sizeof(Data));
    Data jmp_dst = Data_from_label(label);
    Vec_push(&jmp_args, &jmp_dst);
    SRESULT_UNWRAP(
        expand_asmacro("jmp", "", jmp_args, generator, variable_manager, &tmp_data), (void)NULL
    );

    Data_free(tmp_data);

    return SResult_new(NULL);
}

static SResult build_jmp_to_endif(u32 id, inout Generator* generator, inout VariableManager* variable_manager) {
    char label[256];
    snprintf(label, 256, ".%u.if.end", id);

    SRESULT_UNWRAP(
        build_jmp(label, generator, variable_manager), (void)NULL
    );
    
    return SResult_new(NULL);
}

static void Syntax_build_if_build(Parser condition_parser, Parser then_branch, Parser else_branch, inout Generator* generator, inout VariableManager* variable_manager) {
    u32 id = get_id();

    char if_label[256];
    snprintf(if_label, 256, ".%u.if", id);
    if(resolve_sresult(Generator_append_label(generator, ".text", if_label, false, Label_Notype), condition_parser.offset, generator)) {
        return;
    }

    Data condition_data;
    if(resolve_sresult(Syntax_build(condition_parser, generator, variable_manager, &condition_data), condition_parser.offset, generator)) {
        return;
    }
    if(strcmp(condition_data.type.name, "bool") != 0) {
        char msg[256];
        snprintf(msg, 256, "expected bool, found \"%.200s\"", condition_data.type.name);
        Data_free(condition_data);
        Generator_add_error(generator, Error_new(condition_parser.offset, msg));
        return;
    }

    if(resolve_sresult(build_cmpzero(condition_data, generator, variable_manager), condition_parser.offset, generator)
        || resolve_sresult(build_je_to_else(id, generator, variable_manager), condition_parser.offset, generator)
        || resolve_sresult(build_branch(then_branch, id, "then", generator, variable_manager), then_branch.offset, generator)) {
        return;
    }

    if(!Parser_is_empty(&else_branch)) {
        if(resolve_sresult(build_jmp_to_endif(id, generator, variable_manager), then_branch.offset, generator)) {
            return;
        }
    }
    if(resolve_sresult(build_branch(else_branch, id, "else", generator, variable_manager), else_branch.offset, generator)) {
        return;
    }

    char endif_label[256];
    snprintf(endif_label, 256, ".%u.if.end", id);
    if(resolve_sresult(Generator_append_label(generator, ".text", endif_label, false, Label_Notype), else_branch.offset, generator)) {
        return;
    }
}

bool Syntax_build_if(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "if"))) {
        return false;
    }

    *data = Data_void();

    Parser condition_parser;
    Parser if_branch_parser;
    Parser else_branch_parser;
    if(resolve_parsermsg(
        Syntax_build_if_parse(parser, generator, &condition_parser, &if_branch_parser, &else_branch_parser), generator
    )) {
        return true;
    }

    Syntax_build_if_build(condition_parser, if_branch_parser, else_branch_parser, generator, variable_manager);

    return true;
}

static ParserMsg Syntax_build_for_parse(Parser parser, in Generator* generator, out Parser* init_parser, out Parser* cond_parser, out Parser* inc_parser, out Parser* proc_parser) {
    Parser paren_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser, &paren_parser), (void)NULL
    );

    *init_parser = Parser_split(&paren_parser, ";");
    if(Parser_is_empty(&paren_parser)) return ParserMsg_new(paren_parser.offset, "expected symbol \";\"");
    *cond_parser = Parser_split(&paren_parser, ";");
    if(Parser_is_empty(&paren_parser)) return ParserMsg_new(paren_parser.offset, "expected symbol \";\"");
    *inc_parser = paren_parser;

    PARSERMSG_UNWRAP(
        Parser_parse_block(&parser, proc_parser), (void)NULL
    );

    check_parser(&parser, generator);

    return ParserMsg_new(parser.offset, NULL);
}

static void Syntax_build_for_build_init(Parser init_parser, u32 id, inout Generator* generator, inout VariableManager* variable_manager) {
    char start_label[256];
    snprintf(start_label, 256, ".%u.for", id);
    resolve_sresult(
        Generator_append_label(generator, ".text", start_label, false, Label_Notype), init_parser.offset, generator
    );

    Data data;
    if(resolve_sresult(
        Syntax_build(init_parser, generator, variable_manager, &data), init_parser.offset, generator
    )) {
        return;
    }
    Data_free(data);

    return;
}

static void Syntax_build_for_build_condition(Parser cond_parser, u32 id, inout Generator* generator, inout VariableManager* variable_manager) {
    Data data;
    
    char cond_label[256];
    snprintf(cond_label, 256, ".%u.for.cond", id);
    resolve_sresult(
        Generator_append_label(generator, ".text", cond_label, false, Label_Notype), cond_parser.offset, generator
    );

    if(resolve_sresult(
        Syntax_build(cond_parser, generator, variable_manager, &data), cond_parser.offset, generator
    )) {
        return;
    }

    if(strcmp(data.type.name, "bool") != 0) {
        Data_free(data);
        Error error = Error_new(cond_parser.offset, "expected bool");
        Generator_add_error(generator, error);
        return;
    }

    if(resolve_sresult(build_cmpzero(data, generator, variable_manager), cond_parser.offset, generator)) {
        return;
    }

    char label[256];
    snprintf(label, 256, ".%u.for.end", id);
    if(resolve_sresult(build_je(label, generator, variable_manager), cond_parser.offset, generator)) {
        return;
    }
}

static void Syntax_build_for_build_proc(Parser proc_parser, u32 id, inout Generator* generator, inout VariableManager* variable_manager) {
    char proc_label[256];
    snprintf(proc_label, 256, ".%u.for.proc", id);
    resolve_sresult(
        Generator_append_label(generator, ".text", proc_label, false, Label_Notype), proc_parser.offset, generator
    );

    VariableManager_new_block(variable_manager);

    SyntaxTree_build(proc_parser, generator, variable_manager);

    resolve_sresult(
        VariableManager_delete_block(variable_manager, generator), proc_parser.offset, generator
    );
}

static void Syntax_build_for_build_inc(Parser inc_parser, u32 id, inout Generator* generator, inout VariableManager* variable_manager) {
    Data data;
    if(resolve_sresult(Syntax_build(inc_parser, generator, variable_manager, &data), inc_parser.offset, generator)) {
        return;
    }
    Data_free(data);

    char cond_label[256];
    snprintf(cond_label, 256, ".%u.for.cond", id);
    resolve_sresult(
        build_jmp(cond_label, generator, variable_manager), inc_parser.offset, generator
    );
}

static void Syntax_build_for_build(
    Parser init_parser, Parser cond_parser, Parser inc_parser, Parser proc_parser,
    inout Generator* generator,
    inout VariableManager* variable_manager) {

    u32 id = get_id();

    VariableManager_new_block(variable_manager);

    Syntax_build_for_build_init(init_parser, id, generator, variable_manager);
    Syntax_build_for_build_condition(cond_parser, id, generator, variable_manager);
    Syntax_build_for_build_proc(proc_parser, id, generator, variable_manager);
    Syntax_build_for_build_inc(inc_parser, id, generator, variable_manager);

    char end_label[256];
    snprintf(end_label, 256, ".%u.for.end", id);
    resolve_sresult(
        Generator_append_label(generator, ".text", end_label, false, Label_Notype), init_parser.offset, generator
    );

    resolve_sresult(
        VariableManager_delete_block(variable_manager, generator), init_parser.offset, generator
    );
}

bool Syntax_build_for(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    *data = Data_void();

    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "for"))) {
        return false;
    }

    Parser init_parser;
    Parser cond_parser;
    Parser inc_parser;
    Parser proc_parser;
    if(resolve_parsermsg(Syntax_build_for_parse(parser, generator, &init_parser, &cond_parser, &inc_parser, &proc_parser), generator)) {
        return true;
    }

    Syntax_build_for_build(init_parser, cond_parser, inc_parser, proc_parser, generator, variable_manager);

    return true;
}

static ParserMsg Syntax_build_while_parse(Parser parser, out Parser* cond_parser, out Parser* proc_parser) {
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser, cond_parser), (void)NULL
    );
    PARSERMSG_UNWRAP(
        Parser_parse_block(&parser, proc_parser), (void)NULL
    );

    return ParserMsg_new(parser.offset, NULL);
}

static SResult Syntax_build_while_build_cond(u32 id, Parser cond_parser, inout Generator* generator, inout VariableManager* variable_manager) {
    char label[256];
    snprintf(label, 256, ".%u.while", id);

    SRESULT_UNWRAP(
        Generator_append_label(generator, ".text", label, false, Label_Notype), (void)NULL
    );

    Data data;
    SRESULT_UNWRAP(
        Syntax_build(cond_parser, generator, variable_manager, &data), (void)NULL
    )
    if(strcmp(data.type.name, "bool") != 0) {
        Data_free(data);
        return SResult_new("expected bool");
    }

    SRESULT_UNWRAP(
        build_cmpzero(data, generator, variable_manager), (void)NULL
    );

    char while_end[256];
    snprintf(while_end, 256, ".%u.while.end", id);
    SRESULT_UNWRAP(
        build_je(while_end, generator, variable_manager), (void)NULL
    );

    return SResult_new(NULL);
}

static SResult Syntax_build_while_build_proc(u32 id, Parser proc_parser, inout Generator* generator, inout VariableManager* variable_manager) {
    char proc_label[256];
    snprintf(proc_label, 256, ".%u.while.proc", id);
    SRESULT_UNWRAP(
        Generator_append_label(generator, ".text", proc_label, false, Label_Notype), (void)NULL
    );

    VariableManager_new_block(variable_manager);
    
    SyntaxTree_build(proc_parser, generator, variable_manager);

    SRESULT_UNWRAP(
        VariableManager_delete_block(variable_manager, generator), (void)NULL
    );

    char while_start[256];
    snprintf(while_start, 256, ".%u.while", id);
    SRESULT_UNWRAP(
        build_jmp(while_start, generator, variable_manager), (void)NULL
    );

    char end_label[256];
    snprintf(end_label, 256, ".%u.while.end", id);
    SRESULT_UNWRAP(
        Generator_append_label(generator, ".text", end_label, false, Label_Notype), (void)NULL
    );

    return SResult_new(NULL);
}

static void Syntax_build_while_build(Parser cond_parser, Parser proc_parser, inout Generator* generator, inout VariableManager* variable_manager) {
    u32 id = get_id();

    resolve_sresult(Syntax_build_while_build_cond(id, cond_parser, generator, variable_manager), cond_parser.offset, generator);
    resolve_sresult(Syntax_build_while_build_proc(id, proc_parser, generator, variable_manager), proc_parser.offset, generator);
}

bool Syntax_build_while(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    *data = Data_void();

    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "while"))) {
        return false;
    }

    Parser cond_parser;
    Parser proc_parser;
    if(resolve_parsermsg(Syntax_build_while_parse(parser, &cond_parser, &proc_parser), generator)) {
        return true;
    }

    Syntax_build_while_build(cond_parser, proc_parser, generator, variable_manager);
    return true;
}

bool Syntax_build_block(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    *data = Data_void();

    Parser proc_parser;
    if(!ParserMsg_is_success(Parser_parse_block(&parser, &proc_parser))) {
        return false;
    }

    SyntaxTree_build(proc_parser, generator, variable_manager);

    check_parser(&parser, generator);

    return true;
}

static SResult Syntax_build_paren_helper(Parser paren_parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    *data = Data_void();

    while(!Parser_is_empty(&paren_parser)) {
        Data_free(*data);
        Parser syntax_parser = Parser_split(&paren_parser, ";");

        SRESULT_UNWRAP(
            Syntax_build(syntax_parser, generator, variable_manager, data),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

bool Syntax_build_paren(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Parser paren_parser;
    if(!ParserMsg_is_success(Parser_parse_paren(&parser, &paren_parser))) {
        return false;
    }

    if(resolve_sresult(Syntax_build_paren_helper(paren_parser, generator, variable_manager, data), parser.offset, generator)) {
        *data = Data_void();
        return true;
    }

    check_parser(&parser, generator);
    return true;
}

static ParserMsg Syntax_build_assignadd_parse(Parser parser, out Parser* left_parser, out Parser* right_parser) {
    *left_parser = Parser_split(&parser, "+=");
    *right_parser = parser;

    if(Parser_is_empty(left_parser)) {
        return ParserMsg_new(left_parser->offset, "expected left expression");
    }
    if(Parser_is_empty(right_parser)) {
        return ParserMsg_new(right_parser->offset, "expected right expression");
    }

    return ParserMsg_new(parser.offset, NULL);
}

bool Syntax_build_assignadd(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Parser left_parser;
    Parser right_parser;
    if(!ParserMsg_is_success(Syntax_build_assignadd_parse(parser, &left_parser, &right_parser))) {
        return false;
    }

    resolve_sresult(build_assignops("add", left_parser, right_parser, generator, variable_manager), parser.offset, generator);
    *data = Data_void();

    return true;
}

static ParserMsg Syntax_build_assignsub_parse(Parser parser, out Parser* left_parser, out Parser* right_parser) {
    *left_parser = Parser_split(&parser, "-=");
    *right_parser = parser;

    if(Parser_is_empty(left_parser)) {
        return ParserMsg_new(left_parser->offset, "expected left expression");
    }
    if(Parser_is_empty(right_parser)) {
        return ParserMsg_new(right_parser->offset, "expected right expression");
    }

    return ParserMsg_new(parser.offset, NULL);
}

bool Syntax_build_assignsub(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Parser left_parser;
    Parser right_parser;
    if(!ParserMsg_is_success(Syntax_build_assignsub_parse(parser, &left_parser, &right_parser))) {
        return false;
    }

    resolve_sresult(build_assignops("sub", left_parser, right_parser, generator, variable_manager), parser.offset, generator);
    *data = Data_void();

    return true;
}

static ParserMsg Syntax_build_assignand_parse(Parser parser, out Parser* left_parser, out Parser* right_parser) {
    *left_parser = Parser_split(&parser, "&=");
    *right_parser = parser;

    if(Parser_is_empty(left_parser)) {
        return ParserMsg_new(left_parser->offset, "expected left expression");
    }
    if(Parser_is_empty(right_parser)) {
        return ParserMsg_new(right_parser->offset, "expected right expression");
    }

    return ParserMsg_new(parser.offset, NULL);
}

bool Syntax_build_assignand(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Parser left_parser;
    Parser right_parser;
    if(!ParserMsg_is_success(Syntax_build_assignand_parse(parser, &left_parser, &right_parser))) {
        return false;
    }

    resolve_sresult(build_assignops("and", left_parser, right_parser, generator, variable_manager), parser.offset, generator);
    *data = Data_void();

    return true;
}

static ParserMsg Syntax_build_assignor_parse(Parser parser, out Parser* left_parser, out Parser* right_parser) {
    *left_parser = Parser_split(&parser, "|=");
    *right_parser = parser;

    if(Parser_is_empty(left_parser)) {
        return ParserMsg_new(left_parser->offset, "expected left expression");
    }
    if(Parser_is_empty(right_parser)) {
        return ParserMsg_new(right_parser->offset, "expected right expression");
    }

    return ParserMsg_new(parser.offset, NULL);
}

bool Syntax_build_assignor(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Parser left_parser;
    Parser right_parser;
    if(!ParserMsg_is_success(Syntax_build_assignor_parse(parser, &left_parser, &right_parser))) {
        return false;
    }

    resolve_sresult(build_assignops("or", left_parser, right_parser, generator, variable_manager), parser.offset, generator);
    *data = Data_void();

    return true;
}

static ParserMsg Syntax_build_assignxor_parse(Parser parser, out Parser* left_parser, out Parser* right_parser) {
    *left_parser = Parser_split(&parser, "^=");
    *right_parser = parser;

    if(Parser_is_empty(left_parser)) {
        return ParserMsg_new(left_parser->offset, "expected left expression");
    }
    if(Parser_is_empty(right_parser)) {
        return ParserMsg_new(right_parser->offset, "expected right expression");
    }

    return ParserMsg_new(parser.offset, NULL);
}

bool Syntax_build_assignxor(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Parser left_parser;
    Parser right_parser;
    if(!ParserMsg_is_success(Syntax_build_assignxor_parse(parser, &left_parser, &right_parser))) {
        return false;
    }

    resolve_sresult(build_assignops("xor", left_parser, right_parser, generator, variable_manager), parser.offset, generator);
    *data = Data_void();

    return true;
}

static ParserMsg Syntax_build_assignlea_parse(Parser parser, out Parser* left_parser, out Parser* right_parser) {
    *left_parser = Parser_split(&parser, "=&");
    *right_parser = parser;

    if(Parser_is_empty(left_parser)) {
        return ParserMsg_new(left_parser->offset, "expected left expression");
    }
    if(Parser_is_empty(right_parser)) {
        return ParserMsg_new(right_parser->offset, "expected right expression");
    }

    return ParserMsg_new(parser.offset, NULL);
}

bool Syntax_build_assignlea(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Parser left_parser;
    Parser right_parser;
    if(!ParserMsg_is_success(Syntax_build_assignlea_parse(parser, &left_parser, &right_parser))) {
        return false;
    }

    resolve_sresult(build_assignops("lea", left_parser, right_parser, generator, variable_manager), parser.offset, generator);
    *data = Data_void();

    return true;
}

bool Syntax_build_implicit_static_string(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    (void)variable_manager;
    
    Vec string;
    if(!ParserMsg_is_success(Parser_parse_string(&parser, &string))) {
        return false;
    }
    assert(Vec_size(&string) == sizeof(u8));

    *data = Data_void();

    u32 id = get_id();
    char label[256];
    snprintf(label, 256, ".%u.implicit_string", id);

    resolve_sresult(
        Generator_append_label(generator, ".data", label, false, Label_Object),
        parser.offset, generator
    );
    for(u32 i=0; i<Vec_len(&string); i++) {
        u8* byte = Vec_index(&string, i);
        resolve_sresult(
            Generator_append_binary(generator, ".data", *byte),
            parser.offset, generator
        );
    }
    resolve_sresult(
        Generator_end_label(generator, label),
        parser.offset, generator
    );
    u32 len = Vec_len(&string);

    Vec_free(string);
    Type type_char = {"char", "", Type_Integer, {}, 1, 1};
    Type* type_char_boxed = malloc(sizeof(Type));
    UNWRAP_NULL(type_char_boxed);
    *type_char_boxed = type_char;

    Type type_string = {
        "", "", Type_Array,
        {.t_array = {type_char_boxed, len}},
        len,
        1
    };
    snprintf(type_string.name, 256, "[char; %u]", len);

    *data = Data_from_mem(Rip, 0, label, type_string);

    return true;
}

static SResult Syntax_build_unary_ops_helper(
    in char* macro_name, Data operand, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Vec args = Vec_new(sizeof(Data));
    Vec_push(&args, &operand);
    SRESULT_UNWRAP(
        expand_asmacro(macro_name, "", args, generator, variable_manager, data),
        (void)NULL
    );
    return SResult_new(NULL);
}

bool Syntax_build_inc(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    if(!ParserMsg_is_success(Parser_parse_symbol(&parser, "++"))) {
        return false;
    }

    Data syntax_data;
    if(resolve_sresult(
        Syntax_build(parser, generator, variable_manager, &syntax_data), parser.offset, generator
    )) {
        return true;
    }

    if(resolve_sresult(
        Syntax_build_unary_ops_helper("inc", syntax_data, generator, variable_manager, data), parser.offset, generator
    )) {
        *data = Data_void();
        return true;
    }

    return true;
}

bool Syntax_build_dec(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    if(!ParserMsg_is_success(Parser_parse_symbol(&parser, "--"))) {
        return false;
    }

    Data syntax_data;
    if(resolve_sresult(
        Syntax_build(parser, generator, variable_manager, &syntax_data), parser.offset, generator
    )) {
        return true;
    }

    if(resolve_sresult(
        Syntax_build_unary_ops_helper("dec", syntax_data, generator, variable_manager, data), parser.offset, generator
    )) {
        *data = Data_void();
        return true;
    }

    return true;
}

bool Syntax_build_debug_label_syntax(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    (void)variable_manager;
    if(!ParserMsg_is_success(Parser_parse_symbol(&parser, "!"))) {
        return false;
    }
    
    *data = Data_void();

    char ident[256];
    if(resolve_parsermsg(
        Parser_parse_ident(&parser, ident), generator
    )) {
        return true;
    }

    u32 id = get_id();
    char name[256];
    snprintf(name, 256, ".%u.%.100s", id, ident);
    if(resolve_sresult(
       Generator_append_label(generator, ".text", name, true, Label_Notype),
       parser.offset, generator
    )) {
        return true;
    }

    check_parser(&parser, generator);

    return true;
}

bool Syntax_build_return(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "return"))) {
        return false;
    }

    resolve_sresult(
        VariableManager_delete_allblock(variable_manager, generator), parser.offset, generator
    );

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



