#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "types.h"
#include "gen.h"
#include "syntax.h"
#include "util.h"

static SResult expand_asmacro(in char* name, in char* path, Vec arguments, inout Generator* generator, inout VariableManager* variable_manager, out Data* data);
static SResult sub_rsp(i32 value, inout Generator* generator, inout VariableManager* variable_manager);

VariableManager VariableManager_new(i32 stack_offset) {
    VariableManager variable_manager = {Vec_new(sizeof(Variable)), stack_offset};
    return variable_manager;
}

i32* VariableManager_stack_offset(in VariableManager* self) {
    return &self->stack_offset;
}

void VariableManager_push(inout VariableManager* self, Variable variable) {
    for(u32 i=0; i<Vec_len(&self->variables); i++) {
        Variable* ptr = Vec_index(&self->variables, i);
        if(strcmp(ptr->name, variable.name) == 0 || Storage_cmp(&ptr->data.storage, &variable.data.storage)) {
            Variable removed_var;
            Vec_remove(&self->variables, i, &removed_var);
            Variable_free(removed_var);
            i--;
        }
    }
    Vec_push(&self->variables, &variable);
}

SResult VariableManager_get(inout VariableManager* self, in char* name, out Variable* variable) {
    for(u32 i=0; i<Vec_len(&self->variables); i++) {
        Variable* ptr = Vec_index(&self->variables, i);
        if(strcmp(ptr->name, name) == 0) {
            *variable = Variable_clone(ptr);
            return SResult_new(NULL);
        }
    }
    
    char msg[256];
    snprintf(msg, 256, "variable \"%.10s\" is undefiend", name);
    return SResult_new(msg);
}

void VariableManager_print(in VariableManager* self) {
    printf("VariableManager { variables: ");
    Vec_print(&self->variables, Variable_print_for_vec);
    printf(", stack_offset: %d }", self->stack_offset);
}

void VariableManager_print_for_vec(in void* ptr) {
    VariableManager_print(ptr);
}

VariableManager VariableManager_clone(in VariableManager* self) {
    VariableManager variable_manager = {Vec_clone(&self->variables, Variable_clone_for_vec), self->stack_offset};
    return variable_manager;
}

void VariableManager_free(VariableManager self) {
    Vec_free(self.variables);
}

void VariableManager_free_for_vec(inout void* ptr) {
    VariableManager* variable_manager = ptr;
    VariableManager_free(*variable_manager);
}

static bool resolve_parsermsg(ParserMsg msg, inout Generator* generator) {
    if(!ParserMsg_is_success(msg)) {
        Error error = Error_from_parsermsg(msg);
        Generator_add_error(generator, error);
        return true;
    }
    return false;
}

static bool resolve_sresult(SResult result, Offset offset, inout Generator* generator) {
    if(!SResult_is_success(result)) {
        Generator_add_error(generator, Error_from_sresult(offset, result));
        return true;
    }
    return false;
}

static void check_parser(in Parser* parser, inout Generator* generator) {
    if(!Parser_is_empty(parser)) {
        Generator_add_error(generator, Error_new(parser->offset, "unexpected token"));
    }
}

static bool GlobalSyntax_parse_struct_definision(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "struct"))) {
        return false;
    }
    
    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_StructDefinision;
    global_syntax->ok_flag = false;
    
    Type type;
    if(resolve_parsermsg(Type_parse_struct(&parser, generator, &type), generator)) {
        return true;
    }
    
    if(resolve_sresult(Generator_add_type(generator, type), parser.offset, generator)) {
        return true;
    }

    global_syntax->ok_flag = true;

    check_parser(&parser, generator);
    return true;
}

static bool GlobalSyntax_parse_enum_definision(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "enum"))) {
        return false;
    }
    
    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_EnumDefinision;
    global_syntax->ok_flag = false;
    
    Type type;
    if(resolve_parsermsg(Type_parse_enum(&parser, &type), generator)) {
        return true;
    }
    
    if(resolve_sresult(Generator_add_type(generator, type), parser.offset, generator)) {
        return true;
    }
    
    global_syntax->ok_flag = true;

    check_parser(&parser, generator);
    return true;
}

static bool GlobalSyntax_parse_asmacro_definision(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    Parser temp_parser = parser;
    Parser_parse_keyword(&temp_parser, "pub");
    if(!Parser_start_with(&temp_parser, "as")) {
        return false;
    }

    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_AsmacroDefinision;
    global_syntax->ok_flag = false;

    Asmacro asmacro;
    if(resolve_parsermsg(Asmacro_parse(&parser, generator, &asmacro), generator)) {
        return true;
    }

    global_syntax->body.asmacro_definision = Asmacro_clone(&asmacro);
    global_syntax->ok_flag = true;

    if(resolve_sresult(Generator_add_asmacro(generator, asmacro), parser.offset, generator)) {
        return true;
    }

    check_parser(&parser, generator);
    return true;
}

static bool GlobalSyntax_parse_type_alias(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "type"))) {
        return false;
    }

    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_TypeAlias;
    global_syntax->ok_flag = false;

    char name[256];
    if(resolve_parsermsg(Parser_parse_ident(&parser, name), generator)) {
        return true;
    }

    Type type;
    if(resolve_parsermsg(Type_parse(&parser, generator, &type), generator)) {
        return true;
    }

    if(resolve_sresult(Generator_add_type(generator, type), parser.offset, generator)) {
        return true;
    }

    global_syntax->ok_flag = true;
    return true;
}

static SResult GlobalSyntax_parse_import_code(inout Generator* generator, in char module_name[256]) {
    assert(module_name != NULL);
    assert(generator != NULL);

    char path[260];
    snprintf(path, 260, "%.255s.amc", module_name);
    
    if(!Generator_imported(generator, path)) {
        FILE* file = fopen(path, "rb");
        if(file == NULL) {
            return SResult_new("module is not exist");
        }

        u64 file_size = get_file_size(file);
        char* file_str = malloc(file_size + 1);
        UNWRAP_NULL(file_str);

        if(fread(file_str, 1, file_size, file) <= 0) {
            PANIC("something wrong");
        }
        file_str[file_size] = '\0';
        fclose(file);
        Generator_import(generator, path, file_str);
        Parser parser = Parser_new(file_str, path);

        while(!Parser_is_empty(&parser)) {
            Parser syntax_parser = Parser_split(&parser, ";");
            GlobalSyntax global_syntax;
            GlobalSyntax_parse(syntax_parser, generator, &global_syntax);
            GlobalSyntax_free(global_syntax);
        }
    }

    return SResult_new(NULL);
}

static bool GlobalSyntax_parse_import(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    assert(generator != NULL);
    assert(global_syntax != NULL);

    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "import"))) {
        return false;
    }

    global_syntax->ok_flag = false;
    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_Import;

    char module_name[256];
    if(resolve_parsermsg(Parser_parse_ident(&parser, module_name), generator)) {
        return true;
    }
    global_syntax->ok_flag = !resolve_sresult(
        GlobalSyntax_parse_import_code(generator, module_name),
        parser.offset,
        generator
    );

    check_parser(&parser, generator);
    return true;
}

static ParserMsg function_definision_parse_arguments(Parser parser, in Generator* generator, inout VariableManager* variable_manager, inout Vec* arguments) {
    i32 stack_offset = 0;
    
    while(!Parser_is_empty(&parser)) {
        Parser parser_copy = parser;
        i32 dummy = 0;
        Variable variable;
        PARSERMSG_UNWRAP(
            Variable_parse(&parser_copy, generator, &dummy, &variable),
            (void)NULL
        );
        u32 size = variable.data.type.size;
        u32 align = variable.data.type.align;
        stack_offset -= size;
        stack_offset = (stack_offset - align + 1)/align*align;
        
        Variable_free(variable);
        
        PARSERMSG_UNWRAP(
            Variable_parse(&parser, generator, &stack_offset, &variable),
            (void)NULL
        );
        VariableManager_push(variable_manager, Variable_clone(&variable));
        Vec_push(arguments, &variable);
        
        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, ","),
                (void)NULL
            );
        }
    }
    
    return ParserMsg_new(parser.offset, NULL);
}

static bool GlobalSyntax_parse_function_definision(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    bool public_flag = ParserMsg_is_success(Parser_parse_keyword(&parser, "pub"));
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "fn"))) {
        return false;
    }

    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_FunctionDefinision;
    global_syntax->ok_flag = false;

    char name[256];
    Parser paren_parser;
    VariableManager variable_manager = VariableManager_new(8);
    Vec arguments = Vec_new(sizeof(Variable));
    Parser block_parser;

    if(resolve_parsermsg(Parser_parse_ident(&parser, name), generator)
        || resolve_parsermsg(Parser_parse_paren(&parser, &paren_parser), generator)
        || resolve_parsermsg(function_definision_parse_arguments(paren_parser, generator, &variable_manager, &arguments), generator)
        || resolve_parsermsg(Parser_parse_block(&parser, &block_parser), generator)) {
            
        VariableManager_free(variable_manager);
        Vec_free_all(arguments, Variable_free_for_vec);
        return true;
    }

    Asmacro wrapper_asmacro = Asmacro_new_fn_wrapper(name, arguments, (public_flag)?(""):(Parser_path(&parser)));
    if(resolve_sresult(Generator_add_asmacro(generator, wrapper_asmacro), parser.offset, generator)) {
        return true;
    }

    check_parser(&parser, generator);
    
    strcpy(global_syntax->body.function_definision.name, name);
    global_syntax->body.function_definision.variable_manager = variable_manager;
    global_syntax->body.function_definision.proc_parser = block_parser;

    global_syntax->ok_flag = true;
    return true;
}

ParserMsg GlobalSyntax_parse(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    bool (*BUILDERS[])(Parser, inout Generator*, out GlobalSyntax*) = {
        GlobalSyntax_parse_asmacro_definision,
        GlobalSyntax_parse_struct_definision,
        GlobalSyntax_parse_enum_definision,
        GlobalSyntax_parse_type_alias,
        GlobalSyntax_parse_import,
        GlobalSyntax_parse_function_definision
    };
    
    for(u32 i=0; i<LEN(BUILDERS); i++) {
        if(BUILDERS[i](parser, generator, global_syntax)) {
            return ParserMsg_new(parser.offset, NULL);
        }
    }
    
    return ParserMsg_new(parser.offset, "unknown global syntax");
}

void GlobalSyntax_check_asmacro(inout GlobalSyntax* self, inout Generator* generator) {
    (void)generator;
    if(self->type != GlobalSyntax_AsmacroDefinision) {
        return;
    }

    if(self->body.asmacro_definision.type == Asmacro_UserOperator) {
        TODO();
    }
}

static void GlobalSyntax_build_function_definision(inout GlobalSyntax* self, inout Generator* generator) {
    Parser parser = self->body.function_definision.proc_parser;
    VariableManager* variable_manager = &self->body.function_definision.variable_manager;
    
    Label label;
    strcpy(label.name, self->body.function_definision.name);
    label.public_flag = true;
    strcpy(label.section_name, "text");
    if(resolve_sresult(
        Generator_binary_len(generator, "text", &label.offset),
        parser.offset,
        generator
    )) {
        return;
    }
    resolve_sresult(
        Generator_new_label(generator, label),
        parser.offset,
        generator
    );

    while(!Parser_is_empty(&parser)) {
        Parser syntax_parser = Parser_split(&parser, ";");
        Data data;
        
        resolve_sresult(
            Syntax_build(syntax_parser, generator, variable_manager, &data), parser.offset, generator
        );

        Data_free(data);
    }
}

void GlobalSyntax_build(inout GlobalSyntax* self, inout Generator* generator) {
    assert(self != NULL);
    assert(generator != NULL);

    if(!self->ok_flag) {
        return;
    }
    switch(self->type) {
        case GlobalSyntax_StructDefinision:
        case GlobalSyntax_EnumDefinision:
        case GlobalSyntax_TypeAlias:
        case GlobalSyntax_AsmacroDefinision:
        case GlobalSyntax_Import:
            break;
        case GlobalSyntax_FunctionDefinision:
            GlobalSyntax_build_function_definision(self, generator);
            break;
    }
}

void GlobalSyntax_print(in GlobalSyntax* self) {
    printf("GlobalSyntax { offset: ");
    Offset_print(&self->offset);
    printf("ok_flag: %s, type: %d, body: ",
        BOOL_TO_STR(self->ok_flag),
        self->type
    );

    switch(self->type) {
        case GlobalSyntax_StructDefinision:
            printf(".none");
            break;
        case GlobalSyntax_EnumDefinision:
            printf(".none");
            break;
        case GlobalSyntax_AsmacroDefinision:
            printf(".asmacro_definision: ");
            Asmacro_print(&self->body.asmacro_definision);
            break;
        case GlobalSyntax_TypeAlias:
            printf(".none");
            break;
        case GlobalSyntax_FunctionDefinision:
            printf(".function_definision: name: %s, variable_manager: ", self->body.function_definision.name);
            VariableManager_print(&self->body.function_definision.variable_manager);
            break;
        case GlobalSyntax_Import:
            printf(".none");
            break;
    }
    printf(" }");
}

void GlobalSyntax_print_for_vec(in void* ptr) {
    GlobalSyntax_print(ptr);
}

void GlobalSyntax_free(GlobalSyntax self) {
    if(!self.ok_flag) {
        return;
    }
    switch(self.type) {
        case GlobalSyntax_StructDefinision:
        case GlobalSyntax_EnumDefinision:
        case GlobalSyntax_TypeAlias:
        case GlobalSyntax_Import:
            break;
        case GlobalSyntax_AsmacroDefinision:
            Asmacro_free(self.body.asmacro_definision);
            break;
        case GlobalSyntax_FunctionDefinision:
            VariableManager_free(self.body.function_definision.variable_manager);
            break;
    }
}

void GlobalSyntax_free_for_vec(inout void* ptr) {
    GlobalSyntax* global_syntax = ptr;
    GlobalSyntax_free(*global_syntax);
}

GlobalSyntaxTree GlobalSyntaxTree_new() {
    GlobalSyntaxTree tree = {Generator_new(), Vec_new(sizeof(GlobalSyntax))};
    Generator_new_section(&tree.generator, "text");
    return tree;
}

void GlobalSyntaxTree_parse(inout GlobalSyntaxTree* self, Parser parser) {
    Generator_import(&self->generator, Parser_path(&parser), NULL);

    while(!Parser_is_empty(&parser)) {
        Parser syntax_parser = Parser_split(&parser, ";");
        
        GlobalSyntax global_syntax;
        if(!resolve_parsermsg(GlobalSyntax_parse(syntax_parser, &self->generator, &global_syntax), &self->generator)) {
            Vec_push(&self->global_syntaxes, &global_syntax);
        }
    }
}

void GlobalSyntaxTree_check_asmacro(inout GlobalSyntaxTree* self) {
    for(u32 i=0; i<Vec_len(&self->global_syntaxes); i++) {
        GlobalSyntax* global_syntax = Vec_index(&self->global_syntaxes, i);
        GlobalSyntax_check_asmacro(global_syntax, &self->generator);
    }
}

Generator GlobalSyntaxTree_build(inout GlobalSyntaxTree self) {
    for(u32 i=0; i<Vec_len(&self.global_syntaxes); i++) {
        GlobalSyntax* global_syntax = Vec_index(&self.global_syntaxes, i);
        GlobalSyntax_build(global_syntax, &self.generator);
    }
    
    Vec_free_all(self.global_syntaxes, GlobalSyntax_free_for_vec);
    return self.generator;
}

void GlobalSyntaxTree_print(in GlobalSyntaxTree* self) {
    printf("GlobalSyntaxTree { generator: ");
    Generator_print(&self->generator);
    printf(", global_syntaxes: ");
    Vec_print(&self->global_syntaxes, GlobalSyntax_print_for_vec);
    printf(" }");
}

void GlobalSyntaxTree_free(GlobalSyntaxTree self) {
    Generator_free(self.generator);
    Vec_free_all(self.global_syntaxes, GlobalSyntax_free_for_vec);
}

SResult Syntax_build(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    bool (*BUILDERS[])(Parser, inout Generator*, inout VariableManager* variable_manager, out Data*) = {
        Syntax_build_variable_declaration,
        Syntax_build_asmacro_expansion,
        Syntax_build_register_expression,
        Syntax_build_imm_expression,
        Syntax_build_assignment,
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

        VariableManager_push(variable_manager, variable);
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

static SResult Syntax_build_asmacro_expansion_fnwrapper_push_vars_helper(in Data* data, inout VariableManager* variable_manager, inout Generator* generator, bool volatile_flag) {
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

    if(Register_is_volatile(reg) == volatile_flag) {
        Data arg_data = Data_from_register(reg);
        Vec args = Vec_new(sizeof(Data));
        Vec_push(&args, &arg_data);
        Data data;
        SRESULT_UNWRAP(
            expand_asmacro("push", "", args, generator, variable_manager, &data),
            (void)NULL
        );
        variable_manager->stack_offset += 8;

        Data_free(data);
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_pop_vars_helper(in Data* data, inout VariableManager* variable_manager, inout Generator* generator, bool volatile_flag) {
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

static SResult Syntax_build_asmacro_expansion_fnwrapper_push_volatile_vars(inout VariableManager* variable_manager, inout Generator* generator) {
    for(u32 i=0; i<Vec_len(&variable_manager->variables); i++) {
        Variable* variable = Vec_index(&variable_manager->variables, i);
        SRESULT_UNWRAP(
            Syntax_build_asmacro_expansion_fnwrapper_push_vars_helper(&variable->data, variable_manager, generator, true),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_pop_volatile_vars(inout VariableManager* variable_manager, inout Generator* generator) {
    for(i32 i=Vec_len(&variable_manager->variables)-1; 0<=i; i--) {
        Variable* variable = Vec_index(&variable_manager->variables, i);
        SRESULT_UNWRAP(
            Syntax_build_asmacro_expansion_fnwrapper_pop_vars_helper(&variable->data, variable_manager, generator, true),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_push_nonvolatile_args(in Vec* arguments, inout VariableManager* variable_manager, inout Generator* generator) {
    for(u32 i=0; i<Vec_len(arguments); i++) {
        Data* fn_arg = Vec_index(arguments, i);
        SRESULT_UNWRAP(
            Syntax_build_asmacro_expansion_fnwrapper_push_vars_helper(fn_arg, variable_manager, generator, false),
            (void)NULL
        );
    }

    return SResult_new(NULL);
}

static SResult Syntax_build_asmacro_expansion_fnwrapper_pop_nonvolatile_args(in Vec* arguments, inout VariableManager* variable_manager, inout Generator* generator) {
    for(i32 i=Vec_len(arguments)-1; 0<=i; i--) {
        Data* fn_arg = Vec_index(arguments, i);
        SRESULT_UNWRAP(
            Syntax_build_asmacro_expansion_fnwrapper_pop_vars_helper(fn_arg, variable_manager, generator, false),
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
        u32 size = asmacro_argvar_type->size;
        u64 align = asmacro_argvar_type->align;
        Memory* memory = &asmacro_argvar_storage->body.mem;
        assert(memory->base == Rsp);
        assert(memory->disp.type == Disp_Offset);
        i32 push_offset = (variable_manager->stack_offset+align-1)/align*align;
        i32 call_stack_offset = (push_offset - memory->disp.body.offset + 0x0f)/0x10*0x10;
        push_offset = call_stack_offset + memory->disp.body.offset;

        SRESULT_UNWRAP(
            sub_rsp(push_offset+size-variable_manager->stack_offset, generator, variable_manager),
            (void)NULL
        );

        Vec mov_args = Vec_new(sizeof(Data));
        Data mov_arg_dst = Data_from_mem(Rbp, push_offset);
        Data mov_arg_src = Data_clone(Vec_index(arguments, i));
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
        Syntax_build_asmacro_expansion_fnwrapper_push_volatile_vars(variable_manager, generator),
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
        sub_rsp(stack_offset_before_push-variable_manager->stack_offset, generator, variable_manager),
        (void)NULL
    );
    variable_manager->stack_offset = stack_offset_before_push;
    SRESULT_UNWRAP(
        Syntax_build_asmacro_expansion_fnwrapper_pop_nonvolatile_args(arguments, variable_manager, generator),
        (void)NULL
    );
    SRESULT_UNWRAP(
        Syntax_build_asmacro_expansion_fnwrapper_pop_volatile_vars(variable_manager, generator),
        (void)NULL
    );

    variable_manager->stack_offset = stack_offset;

    return SResult_new(NULL);
}

static SResult expand_asmacro(in char* name, in char* path, Vec arguments, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
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

    switch(asmacro.type) {
        case Asmacro_AsmOperator:
            SRESULT_UNWRAP(
                Syntax_build_asmacro_expansion_asmoperator(&asmacro, &arguments, generator),
                Asmacro_free(asmacro);Vec_free_all(arguments, Data_free_for_vec)
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
        *data = Data_clone(Vec_index(&arguments, Vec_len(&arguments)-1));
    }else {
        *data = Data_void();
    }
    Asmacro_free(asmacro);
    Vec_free_all(arguments, Data_free_for_vec);

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
    if(resolve_parsermsg(Variable_parse(&parser, generator, &stack_offset, &variable), generator)) {
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

    variable_manager->stack_offset = stack_offset;
    VariableManager_push(variable_manager, variable);
    
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

bool Syntax_build_assignment(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    Parser left_parser = Parser_split(&parser, "=");
    Parser right_parser = parser;
    if(Parser_is_empty(&right_parser)) {
        return false;
    }
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
    resolve_sresult(expand_asmacro("mov", Parser_path(&parser), mov_args, generator, variable_manager, data), parser.offset, generator);

    return true;
}

bool Syntax_build_variable_expression(Parser parser, inout Generator* generator, inout VariableManager* variable_manager, out Data* data) {
    (void)generator;
    
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





