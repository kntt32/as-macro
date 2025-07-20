#include <stdio.h>
#include <assert.h>
#include "types.h"
#include "register.h"
#include "parser.h"
#include "gen.h"
#include "path.h"
#include "util.h"

SResult Section_new(in char* name, out Section* section) {
    if(!(strlen(name) < 256)) {
        return SResult_new("section name must be shorter than 256 bytes");
    }

    strcpy(section->name, name);
    section->binary = Vec_new(sizeof(u8));

    return SResult_new(NULL);
}

void Section_print(in Section* self) {
    printf("Section { name: %s, binary: ", self->name);
    Vec_print(&self->binary, u8_print_for_vec);
    printf(" }");
}

void Section_print_for_vec(in void* ptr) {
    Section_print(ptr);
}

void Section_free(Section self) {
    Vec_free(self.binary);
}

void Section_free_for_vec(inout void* ptr) {
    Section* section = ptr;
    Section_free(*section);
}

void LabelType_print(LabelType type) {
    switch(type) {
        case Label_Func:
            printf("Label_Func");
            break;
        case Label_Object:
            printf("Label_Object");
            break;
        case Label_Notype:
            printf("Label_Notype");
            break;
    }
}

void Label_print(in Label* self) {
    printf("Label { name: %s, public_flag: %s, section_name: %s, offset: %u, type: ",
        self->name,
        BOOL_TO_STR(self->public_flag),
        self->section_name,
        self->offset
    );
    LabelType_print(self->type);
    printf(", size: %u }", self->size);
}

void Label_print_for_vec(in void* ptr) {
    Label_print(ptr);
}

void Rela_print(in Rela* self) {
    printf("Rela { label: %s, section_name: %s, offset: %u, addend: %d, flag: %s }",
        self->label,
        self->section_name,
        self->offset,
        self->addend,
        BOOL_TO_STR(self->flag)
    );
}

void Rela_print_for_vec(in void* ptr) {
    Rela_print(ptr);
}

void Import_print(in Import* self) {
    assert(self != NULL);
    printf("%s", self->path);
}

void Import_print_for_vec(in void* ptr) {
    Import_print(ptr);
}

void Import_free(Import self) {
    free(self.src);
}

void Import_free_for_vec(inout void* ptr) {
    Import* import = ptr;
    Import_free(*import);
}

Error Error_new(Offset offset, in char* msg) {
    Error error;
    error.offset = offset;
    strncpy(error.msg, msg, 255);
    error.msg[255] = '\0';

    return error;
}

Error Error_from_parsermsg(ParserMsg parser_msg) {
    return Error_new(parser_msg.offset, parser_msg.msg);
}

Error Error_from_sresult(Offset offset, SResult result) {
    return Error_new(offset, result.error);
}

void Error_print(in Error* self) {
    printf("Error { Offset: ");
    Offset_print(&self->offset);
    printf(", msg: %s }", self->msg);
}

void Error_print_for_vec(in void* ptr) {
    Error_print(ptr);
}

Generator Generator_new(Vec import_paths) {
    Generator generator = {
        Type_primitives(),
        Vec_new(sizeof(Variable)),
        Vec_new(sizeof(Asmacro)),
        Vec_new(sizeof(Error)),
        Vec_new(sizeof(Import)),
        import_paths,
        Vec_new(sizeof(Asmacro)),
        "",
        Vec_new(sizeof(Section)),
        Vec_new(sizeof(Label)),
        Vec_new(sizeof(Rela))
    };
    return generator;
}

static bool already_imported(in Generator* self, in char* module_realpath) {
    for(u32 i=0; i<Vec_len(&self->imports); i++) {
        Import* import = Vec_index(&self->imports, i);
        if(strcmp(import->path, module_realpath) == 0) {
            return true;
        }
    }

    return false;
}

SResult Generator_import_by_path(inout Generator* self, in char* path, out bool* doubling_flag, out Parser* optional parser) {
    if(already_imported(self, path)) {
        *doubling_flag = true;
        return SResult_new(NULL);
    }

    *doubling_flag = false;
    Import import;
    char added_path[4096];
    snprintf(added_path, 4096, "%.4095s", path);
    if(path_real(added_path, import.path) == NULL) {
        return SResult_new("too long module name");
    }
    if(!SResult_is_success(map_file(path, &import.src))) {
        return SResult_new("no such a module");
    }

    *parser = Parser_new(import.src, import.path);
    Vec_push(&self->imports, &import);
    return SResult_new(NULL);
}

static bool import_rel(inout Generator* self, in char* parent_path, in char* child_path, out bool* doubling_flag, out Parser* optional parser) {
    char added_path[4096];
    char real_path[4096];
    snprintf(added_path, 4096, "%.4095s", parent_path);
    if(path_super(added_path) == NULL || path_child(added_path, child_path) == NULL) {
        return false;
    }
    if(path_real(added_path, real_path) == NULL) {
        return false;
    }

    if(!SResult_is_success(Generator_import_by_path(self, real_path, doubling_flag, parser))) {
        return false;
    }

    return true;
}

ParserMsg Generator_import(inout Generator* self, Parser parser, out bool* doubling_flag, out Parser* optional import_parser) {
    char module_path[4096] = ".";
    while(!Parser_is_empty(&parser)) {
        char token[256];
        PARSERMSG_UNWRAP(
            Parser_parse_ident(&parser, token), (void)NULL
        );

        if(strcmp(token, "super") == 0) {
            if(path_super(module_path) == NULL && path_append_super(module_path) == NULL) {
                return ParserMsg_new(parser.offset, "too long module path");
            }
        }else {
            if(path_child(module_path, token) == NULL) {
                return ParserMsg_new(parser.offset, "too long module path");
            }
        }

        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, "."), (void)NULL
            );
        }
    }
    if(path_append_extension(module_path, "amc") == NULL) {
        return ParserMsg_new(parser.offset, "too long module path");
    }

    if(import_rel(self, Parser_path(&parser), module_path, doubling_flag, import_parser)) {
        return ParserMsg_new(parser.offset, NULL);
    }

    for(u32 i=0; i<Vec_len(&self->import_paths); i++) {
        char** import_path_ptr = Vec_index(&self->import_paths, i);
        char* import_path = *import_path_ptr;

        char module_added_path[4096] = "";
        snprintf(module_added_path, 4096, "%.3000s/%.1094s", import_path, module_path);
        char module_realpath[4096];
        if(path_real(module_added_path, module_realpath) == NULL) {
            continue;
        }
        
        if(SResult_is_success(Generator_import_by_path(self, module_realpath, doubling_flag, import_parser))) {
            return ParserMsg_new(parser.offset, NULL);
        }
    }

    return ParserMsg_new(parser.offset, "module not found");
}
/*
bool Generator_imported(in Generator* self, in char module_path[256]) {
    assert(self != NULL && module_path != NULL);

    for(u32 i=0; i<Vec_len(&self->imports); i++) {
        Import* ptr = Vec_index(&self->imports, i);

        if(strcmp(ptr->path, module_path) == 0) {
            return true;
        }
    }

    return false;
}

void Generator_import(inout Generator* self, in char module_path[256], optional char* src) {
    Import import;

    strcpy(import.path, module_path);
    import.src = src;

    Vec_push(&self->imports, &import);
}
*/
SResult Generator_add_type(inout Generator* self, Type type) {
    for(u32 i=0; i<Vec_len(&self->types); i++) {
        Type* ptr = Vec_index(&self->types, i);
        if(strcmp(ptr->name, type.name) == 0) {
            Type_free(type);
            char msg[256];
            snprintf(msg, 256, "type \"%.10s\" has been already defined", type.name);
            return SResult_new(msg);
        }
    }

    Vec_push(&self->types, &type);

    return SResult_new(NULL);
}

SResult Generator_get_type(in Generator* self, in char* name, out Type* type) {
    Type* ptr = NULL;
    for(u32 i=0; i<Vec_len(&self->types); i++) {
        ptr = Vec_index(&self->types, i);
        if(strcmp(ptr->name, name) == 0) {
            *type = Type_clone(ptr);
            return SResult_new(NULL);
        }
    }

    char msg[256];
    snprintf(msg, 255, "type \"%.10s\" is undefined", name);
    return SResult_new(msg);
}

SResult Generator_add_global_variable(inout Generator* self, Variable variable) {
    for(u32 i=0; i<Vec_len(&self->global_variables); i++) {
        Variable* ptr = Vec_index(&self->global_variables, i);
        if(strcmp(ptr->name, variable.name) == 0) {
            Variable_free(variable);
            char msg[256];
            snprintf(msg, 256, "variable \"%.10s\" has been already defined", variable.name);
            return SResult_new(msg);
        }
    }

    Vec_push(&self->global_variables, &variable);

    return SResult_new(NULL);
}

SResult Generator_get_global_variable(in Generator* self, in char* path, in char* name, out Variable* variable) {
    for(u32 i=0; i<Vec_len(&self->global_variables); i++) {
        Variable* ptr = Vec_index(&self->global_variables, i);
        if(ptr->valid_path[0] != '\0' && strcmp(ptr->valid_path, path) != 0) {
            continue;
        }
        if(strcmp(ptr->name, name) == 0) {
            *variable = Variable_clone(ptr);
            return SResult_new(NULL);
        }
    }

    char msg[256];
    snprintf(msg, 256, "variable \"%.200s\" is undefined", name);
    return SResult_new(msg);
}

SResult Generator_add_asmacro(inout Generator* self, Asmacro asmacro) {
    for(u32 i=0; i<Vec_len(&self->asmacroes); i++) {
        Asmacro* ptr = Vec_index(&self->asmacroes, i);
        if(Asmacro_cmp_signature(ptr, &asmacro)) {
            if(ptr->type == Asmacro_FnExtern) {
                Asmacro_free(*ptr);
                *ptr = asmacro;
                return SResult_new(NULL);
            }else {
                Asmacro_free(asmacro);
                char msg[256];
                snprintf(msg, 256, "asmacro \"%.10s\" has been already defined in same signature", asmacro.name);
                return SResult_new(msg);
            }
        }
    }

    Vec_push(&self->asmacroes, &asmacro);
    return SResult_new(NULL);
}

SResult Generator_get_asmacro(in Generator* self, in char* name, out Vec* asmacroes) {
    *asmacroes = Vec_new(sizeof(Asmacro));

    for(u32 i=0; i<Vec_len(&self->asmacroes); i++) {
        Asmacro* asmacro_ptr = Vec_index(&self->asmacroes, i);

        if(strcmp(asmacro_ptr->name, name) == 0) {
            Asmacro asmacro = Asmacro_clone(asmacro_ptr);
            Vec_push(asmacroes, &asmacro);
        }
    }

    if(Vec_len(asmacroes) == 0) {
        Vec_free(*asmacroes);
        char msg[256];
        snprintf(msg, 256, "asmacro \"%.10s\" is undefined", name);
        return SResult_new(msg);
    }

    return SResult_new(NULL);
}

void Generator_add_error(inout Generator* self, Error error) {
    Vec_push(&self->errors, &error);
}

SResult Generator_new_section(inout Generator* self, in char* name) {
    for(u32 i=0; i<Vec_len(&self->sections); i++) {
        Section* section = Vec_index(&self->sections, i);
        if(strcmp(section->name, name) == 0) {
            char msg[256];
            snprintf(msg, 256, "section \"%.10s\" has been already defined", name);
            return SResult_new(msg);
        }
    }

    Section section;
    SRESULT_UNWRAP(
        Section_new(name, &section), (void)NULL
    );

    Vec_push(&self->sections, &section);

    return SResult_new(NULL);
}

SResult Generator_new_label(inout Generator* self, Label label) {
    Vec_push(&self->labels, &label);
    return SResult_new(NULL);
}

SResult Generator_append_label(inout Generator* self, optional in char* section, in char* name, bool global_flag, LabelType type) {
    Label label;
    if(section != NULL) {
        if(name[0] == '.') {
            snprintf(label.name, sizeof(label.name), "%.127s%.127s", self->namespace, name);
        }else {
            snprintf(self->namespace, 256, "%.255s", name);
            snprintf(label.name, 256, "%.255s", name);
        }
    }else {
        snprintf(label.name, 256, "%.255s", name);
    }
    if(section != NULL) {
        snprintf(label.section_name, 256, "%.255s", section);
        SRESULT_UNWRAP(
            Generator_binary_len(self, section, &label.offset),
            (void)NULL
        );
    }else {
        label.section_name[0] = '\0';
        label.offset = 0;
    }
    label.public_flag = global_flag;
    label.type = type;
    label.size = 0;

    for(u32 i=0; i<Vec_len(&self->labels); i++) {
        Label* ptr = Vec_index(&self->labels, i);
        if(strcmp(ptr->name, label.name) == 0) {
            if(label.section_name[0] != '\0') {
                *ptr = label;
            }
            return SResult_new(NULL);
        }
    }

    Vec_push(&self->labels, &label);
    return SResult_new(NULL);
}

SResult Generator_end_label(inout Generator* self, in char* name) {
    char resolved_name[256];
    if(name[0] == '.') {
        snprintf(resolved_name, 256, "%.100s%.155s", self->namespace, name);
    }else {
        snprintf(resolved_name, 256, "%.255s", name);
    }

    for(u32 i=0; i<Vec_len(&self->labels); i++) {
        Label* label = Vec_index(&self->labels, i);

        if(strcmp(label->name, resolved_name) == 0) {
            u32 binary_len = 0;
            SRESULT_UNWRAP(
                Generator_binary_len(self, label->section_name, &binary_len),
                (void)NULL
            );

            label->size = binary_len - label->offset;
            return SResult_new(NULL);
        }
    }

    return SResult_new("unknown section");
}

SResult Generator_append_rela(inout Generator* self, in char* section, in char* label, bool flag) {
    Rela rela;
    if(label[0] == '.') {
        snprintf(rela.label, sizeof(rela.label), "%.127s%.127s", self->namespace, label);
    }else {
        snprintf(rela.label, sizeof(rela.label), "%.255s", label);
    }
    snprintf(rela.section_name, sizeof(rela.section_name), "%.255s", section);
    rela.addend = 0;
    rela.flag = flag;

    SRESULT_UNWRAP(
        Generator_binary_len(self, section, &rela.offset),
        (void)NULL
    );

    Vec_push(&self->relas, &rela);

    return SResult_new(NULL);
}

SResult Generator_addend_rela(inout Generator* self, in char* section) {
    for(u32 i=0; i<Vec_len(&self->relas); i++) {
        Rela* rela = Vec_index(&self->relas, i);
        if(strcmp(rela->section_name, section) == 0 && !rela->flag) {
            u32 section_len = 0;
            SRESULT_UNWRAP(
                Generator_binary_len(self, section, &section_len),
                (void)NULL
            );
            rela->addend = section_len - rela->offset - 0x8;
            rela->flag = true;
        }
    }

    return SResult_new(NULL);
}

static SResult Generator_get_section(in Generator* self, in char* name, out Section** section) {
    for(u32 i=0; i<Vec_len(&self->sections); i++) {
        *section = Vec_index(&self->sections, i);
        if(strcmp((*section)->name, name) == 0) {
            return SResult_new(NULL);
        }
    }

    char msg[256];
    snprintf(msg, 256, "section \"%.10s\" is undefined", name);
    return SResult_new(msg);
}

SResult Generator_append_binary(inout Generator* self, in char* name, u8 byte) {
    Section* section = NULL;
    SRESULT_UNWRAP(
        Generator_get_section(self, name, &section),
        (void)NULL
    );
    Vec_push(&section->binary, &byte);
    return SResult_new(NULL);
}

SResult Generator_last_binary(in Generator* self, in char* name, out u8** byte) {
    Section* section = NULL;
    SRESULT_UNWRAP(
        Generator_get_section(self, name, &section),
        (void)NULL
    );
    SRESULT_UNWRAP(
        Vec_last_ptr(&section->binary, (void**)byte),
        (void)NULL
    );
    return SResult_new(NULL);
}

SResult Generator_binary_len(in Generator* self, in char* name, out u32* len) {
    Section* section = NULL;
    SRESULT_UNWRAP(
        Generator_get_section(self, name, &section),
        (void)NULL
    );

    *len = Vec_len(&section->binary);

    return SResult_new(NULL);
}

SResult Generator_asmacro_expand_check(inout Generator* self, Asmacro asmacro) {
    assert(self != NULL);
    
    for(u32 i=0; i<Vec_len(&self->expand_stack); i++) {
        Asmacro* ptr = Vec_index(&self->expand_stack, i);
        if(Asmacro_cmp_signature(ptr, &asmacro)) {
            Asmacro_free(asmacro);
            char msg[256];
            snprintf(msg, 256, "asmacro \"%.200s\" expand to infinite size", asmacro.name);
            return SResult_new(msg);
        }
    }
    
    Vec_push(&self->expand_stack, &asmacro);
    
    return SResult_new(NULL);
}

void Generator_finish_asmacro_expand(inout Generator* self) {
    assert(self != NULL);
    
    Asmacro asmacro;
    Vec_pop(&self->expand_stack, &asmacro);
    Asmacro_free(asmacro);
}

bool Generator_is_error(in Generator* self) {
    return Vec_len(&self->errors) != 0;
}

void Generator_print(in Generator* self) {
    printf("Generator { types: ");
    Vec_print(&self->types, Type_print_for_vec);
    printf(", global_variables: ");
    Vec_print(&self->global_variables, Variable_print_for_vec);
    printf(", asmacroes: ");
    Vec_print(&self->asmacroes, Asmacro_print_for_vec);
    printf(", errors: ");
    Vec_print(&self->errors, Error_print_for_vec);
    printf(", imports: ");
    Vec_print(&self->imports, Import_print_for_vec);
    printf(", expand_stack: ");
    Vec_print(&self->expand_stack, Asmacro_print_for_vec);
    printf(", sections: ");
    Vec_print(&self->sections, Section_print_for_vec);
    printf(", labels: ");
    Vec_print(&self->labels, Label_print_for_vec);
    printf(", relas: ");
    Vec_print(&self->relas, Rela_print_for_vec);
    printf(" }");
}

void Generator_free(Generator self) {
    Vec_free_all(self.types, Type_free_for_vec);
    Vec_free_all(self.global_variables, Variable_free_for_vec);
    Vec_free_all(self.asmacroes, Asmacro_free_for_vec);
    Vec_free(self.errors);
    Vec_free_all(self.imports, Import_free_for_vec);
    Vec_free(self.import_paths);
    Vec_free_all(self.expand_stack, Asmacro_free_for_vec);
    Vec_free_all(self.sections, Section_free_for_vec);
    Vec_free(self.labels);
    Vec_free(self.relas);
}

void Generator_free_for_vec(inout void* ptr) {
    Generator* generator = ptr;
    Generator_free(*generator);
}

bool resolve_sresult(SResult result, Offset offset, inout Generator* generator) {
    if(!SResult_is_success(result)) {
        Generator_add_error(generator, Error_from_sresult(offset, result));
        return true;
    }
    return false;
}

bool resolve_parsermsg(ParserMsg msg, inout Generator* generator) {
    if(!ParserMsg_is_success(msg)) {
        Error error = Error_from_parsermsg(msg);
        Generator_add_error(generator, error);
        return true;
    }
    return false;
}
