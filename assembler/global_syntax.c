#include <stdio.h>
#include <assert.h>
#include "global_syntax.h"
#include "util.h"

static bool GlobalSyntax_parse_struct_definision(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    bool public_flag = ParserMsg_is_success(Parser_parse_keyword(&parser, "pub"));

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

    if(!public_flag) {
        Type_restrict_namespace(&type, Parser_path(&parser));
    }
    
    if(resolve_sresult(Generator_add_type(generator, type), parser.offset, generator)) {
        return true;
    }

    global_syntax->ok_flag = true;

    check_parser(&parser, generator);
    return true;
}

static bool GlobalSyntax_parse_enum_definision(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    bool public_flag = ParserMsg_is_success(Parser_parse_keyword(&parser, "pub"));

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
    
    if(!public_flag) {
        Type_restrict_namespace(&type, Parser_path(&parser));
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
    bool public_flag = ParserMsg_is_success(Parser_parse_keyword(&parser, "pub"));

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

    type = Type_as_alias(type, name);
    if(!public_flag) {
        Type_restrict_namespace(&type, Parser_path(&parser));
    }

    if(resolve_sresult(Generator_add_type(generator, type), parser.offset, generator)) {
        return true;
    }

    global_syntax->ok_flag = true;
    return true;
}

static ParserMsg GlobalSyntax_parse_import_code(inout Generator* generator, Offset offset, in char module_name[256]) {
    assert(module_name != NULL);
    assert(generator != NULL);

    char path[260];
    snprintf(path, 260, "%.255s.amc", module_name);
    
    if(!Generator_imported(generator, path)) {
        FILE* file = fopen(path, "rb");
        if(file == NULL) {
            return ParserMsg_new(offset, "module is not exist");
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
            PARSERMSG_UNWRAP(
                GlobalSyntax_parse(syntax_parser, generator, &global_syntax), (void)NULL
            );
            GlobalSyntax_free(global_syntax);
        }
    }

    return ParserMsg_new(offset, NULL);
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
    global_syntax->ok_flag = !resolve_parsermsg(
        GlobalSyntax_parse_import_code(generator, parser.offset, module_name),
        generator
    );

    check_parser(&parser, generator);
    return true;
}

static ParserMsg function_definision_parse_arguments(Parser parser, in Generator* generator, inout VariableManager* variable_manager, inout Vec* arguments) {
    i32 stack_offset = 16;
    
    while(!Parser_is_empty(&parser)) {
        Parser parser_copy = parser;
        i32 dummy = 0;
        Variable variable;
        PARSERMSG_UNWRAP(
            Variable_parse(&parser_copy, generator, &dummy, &variable),
            (void)NULL
        );
        if(variable.data.storage.type == StorageType_mem) {
            i32 size = variable.data.type.size;
            i32 align = variable.data.type.align;
            stack_offset = (stack_offset + align - 1)/align*align;
            stack_offset += size;
        }
        
        Variable_free(variable);

        dummy = stack_offset;
        PARSERMSG_UNWRAP(
            Variable_parse(&parser, generator, &dummy, &variable),
            (void)NULL
        );

        resolve_sresult(
            VariableManager_push(variable_manager, Variable_clone(&variable), generator),
            parser.offset,
            generator
        );
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
    VariableManager variable_manager = VariableManager_new(0);
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

    char* valid_path = (public_flag)?(""):(Parser_path(&parser));

    Variable variable = Variable_from_function(name, valid_path, &arguments);
    if(resolve_sresult(Generator_add_global_variable(generator, variable), parser.offset, generator)) {
        Vec_free_all(arguments, Argument_free_for_vec);
        return true;
    }

    Asmacro wrapper_asmacro = Asmacro_new_fn_wrapper(name, arguments, valid_path);
    if(resolve_sresult(Generator_add_asmacro(generator, wrapper_asmacro), parser.offset, generator)) {
        VariableManager_free(variable_manager);
        return true;
    }

    check_parser(&parser, generator);
    
    strcpy(global_syntax->body.function_definision.name, name);
    global_syntax->body.function_definision.public_flag = public_flag;
    global_syntax->body.function_definision.variable_manager = variable_manager;
    global_syntax->body.function_definision.proc_parser = block_parser;

    global_syntax->ok_flag = true;
    return true;
}

static bool GlobalSyntax_parse_function_extern(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    // (pub) extern fn $name ( args );
    bool public_flag = ParserMsg_is_success(Parser_parse_keyword(&parser, "pub"));
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "extern"))
        || !ParserMsg_is_success(Parser_parse_keyword(&parser, "fn"))) {
        return false;
    }

    global_syntax->ok_flag = false;
    global_syntax->offset = parser.offset;
    global_syntax->type = GlobalSyntax_FunctionExtern;

    char name[256];
    Parser paren_parser;
    Vec arguments = Vec_new(sizeof(Variable));
    VariableManager variable_manager = VariableManager_new(0);
    if(resolve_parsermsg(Parser_parse_ident(&parser, name), generator)
        || resolve_parsermsg(Parser_parse_paren(&parser, &paren_parser), generator)
        || resolve_parsermsg(function_definision_parse_arguments(paren_parser, generator, &variable_manager, &arguments), generator)) {
        Vec_free_all(arguments, Variable_free_for_vec);
        VariableManager_free(variable_manager);
        return true;
    }
    VariableManager_free(variable_manager);

    char* valid_path = (public_flag)?(""):(Parser_path(&parser));
    Asmacro extern_asmacro = Asmacro_new_fn_extern(name, arguments, valid_path);
    if(resolve_sresult(Generator_add_asmacro(generator, extern_asmacro), parser.offset, generator)) {
        return true;
    }
    check_parser(&parser, generator);

    return true;
}

static bool GlobalSyntax_parse_static_variable(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    // (pub) static $name: $type (= expr);

    bool public_flag = ParserMsg_is_success(Parser_parse_keyword(&parser, "pub"));
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "static"))) {
        return false;
    }

    global_syntax->ok_flag = false;
    global_syntax->type = GlobalSyntax_StaticVariable;

    if(resolve_parsermsg(
        Variable_parse_static(&parser, public_flag, generator), generator
    )) {
        return true;
    }

    check_parser(&parser, generator);

    global_syntax->ok_flag = true;
    return true;
}

static bool GlobalSyntax_parse_const_variable(Parser parser, inout Generator* generator, out GlobalSyntax* global_syntax) {
    // (pub) const $name: $type = expr;

    bool public_flag = ParserMsg_is_success(Parser_parse_keyword(&parser, "pub"));
    if(!ParserMsg_is_success(Parser_parse_keyword(&parser, "const"))) {
        return false;
    }

    global_syntax->ok_flag = false;
    global_syntax->type = GlobalSyntax_ConstVariable;

    if(resolve_parsermsg(
        Variable_parse_const(&parser, public_flag, generator), generator
    )) {
        return true;
    }

    check_parser(&parser, generator);

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
        GlobalSyntax_parse_function_definision,
        GlobalSyntax_parse_function_extern,
        GlobalSyntax_parse_static_variable,
        GlobalSyntax_parse_const_variable
    };
    
    for(u32 i=0; i<LEN(BUILDERS); i++) {
        if(BUILDERS[i](parser, generator, global_syntax)) {
            return ParserMsg_new(parser.offset, NULL);
        }
    }
    
    return ParserMsg_new(parser.offset, "unknown global syntax");
}

static SResult enter_function(inout Generator* generator, inout VariableManager* variable_manager) {
    Data data;

    Vec push_rbp_args = Vec_new(sizeof(Data));
    Data data_push_rbp = Data_from_register(Rbp);
    Vec_push(&push_rbp_args, &data_push_rbp);
    SRESULT_UNWRAP(
        expand_asmacro("push", "", push_rbp_args, generator, variable_manager, &data),
        (void)NULL
    );
    Data_free(data);

    Vec mov_rbp_rsp_args = Vec_new(sizeof(Data));
    Data data_mov_rbp = Data_from_register(Rbp);
    Data data_mov_rsp = Data_from_register(Rsp);
    Vec_push(&mov_rbp_rsp_args, &data_mov_rbp);
    Vec_push(&mov_rbp_rsp_args, &data_mov_rsp);
    SRESULT_UNWRAP(
        expand_asmacro("mov", "", mov_rbp_rsp_args, generator, variable_manager, &data),
        (void)NULL
    );
    Data_free(data);

    return SResult_new(NULL);
}

static SResult leave_function(inout Generator* generator, inout VariableManager* variable_manager) {
    Data data;

    Vec mov_rsp_rbp_args = Vec_new(sizeof(Data));
    Data data_mov_rsp = Data_from_register(Rsp);
    Data data_mov_rbp = Data_from_register(Rbp);
    Vec_push(&mov_rsp_rbp_args, &data_mov_rsp);
    Vec_push(&mov_rsp_rbp_args, &data_mov_rbp);
    SRESULT_UNWRAP(
        expand_asmacro("mov", "", mov_rsp_rbp_args, generator, variable_manager, &data),
        (void)NULL
    );
    Data_free(data);

    Vec pop_rbp_args = Vec_new(sizeof(Data));
    Data data_pop_rbp = Data_from_register(Rbp);
    Vec_push(&pop_rbp_args, &data_pop_rbp);
    SRESULT_UNWRAP(
        expand_asmacro("pop", "", pop_rbp_args, generator, variable_manager, &data),
        (void)NULL
    );
    Data_free(data);

    return SResult_new(NULL);
}

static void GlobalSyntax_build_function_definision(inout GlobalSyntax* self, inout Generator* generator) {
    Parser parser = self->body.function_definision.proc_parser;
    VariableManager* variable_manager = &self->body.function_definision.variable_manager;
    VariableManager_new_block(variable_manager);
    
    resolve_sresult(
        Generator_append_label(generator, ".text", self->body.function_definision.name, self->body.function_definision.public_flag, Label_Func),
        parser.offset,
        generator
    );
    resolve_sresult(
        enter_function(generator, variable_manager),
        parser.offset,
        generator
    );

    while(!Parser_is_empty(&parser)) {
        Parser syntax_parser = Parser_split(&parser, ";");
        Data data;
        
        if(!resolve_sresult(
            Syntax_build(syntax_parser, generator, variable_manager, &data), parser.offset, generator
        )) {
            Data_free(data);
        }
    }

    resolve_sresult(
        Generator_append_label(generator, ".text", ".ret", false, Label_Notype),
        parser.offset, generator
    );

    resolve_sresult(
        VariableManager_delete_block(variable_manager, generator),
        parser.offset, generator
    );

    resolve_sresult(
        leave_function(generator, variable_manager), parser.offset, generator
    );
    Data data;
    if(!resolve_sresult(
        expand_asmacro("ret", "", Vec_new(sizeof(Data)), generator, variable_manager, &data),
        parser.offset, generator
    )) {
        Data_free(data);
    }

    resolve_sresult(
        Generator_end_label(generator, self->body.function_definision.name),
        parser.offset, generator
    );
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
        case GlobalSyntax_StaticVariable:
        case GlobalSyntax_ConstVariable:
        case GlobalSyntax_FunctionExtern:
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
            printf("none");
            break;
        case GlobalSyntax_EnumDefinision:
            printf("none");
            break;
        case GlobalSyntax_AsmacroDefinision:
            printf(".asmacro_definision: ");
            Asmacro_print(&self->body.asmacro_definision);
            break;
        case GlobalSyntax_TypeAlias:
            printf("none");
            break;
        case GlobalSyntax_FunctionDefinision:
            printf(".function_definision: name: %s, public_flag: %s, variable_manager: ", self->body.function_definision.name, BOOL_TO_STR(self->body.function_definision.public_flag));
            VariableManager_print(&self->body.function_definision.variable_manager);
            break;
        case GlobalSyntax_FunctionExtern:
            printf("none");
            break;
        case GlobalSyntax_Import:
            printf("none");
            break;
        case GlobalSyntax_StaticVariable:
            printf("none");
            break;
        case GlobalSyntax_ConstVariable:
            printf("none");
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
        case GlobalSyntax_ConstVariable:
        case GlobalSyntax_StaticVariable:
        case GlobalSyntax_FunctionExtern:
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
    Generator_new_section(&tree.generator, ".text");
    Generator_new_section(&tree.generator, ".data");
    Generator_new_section(&tree.generator, ".bss");
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

