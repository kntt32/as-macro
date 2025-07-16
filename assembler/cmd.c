#include <stdio.h>
#include "types.h"
#include "vec.h"
#include "gen.h"
#include "gen_elf.h"
#include "global_syntax.h"
#include "util.h"
#include "cmd.h"

Args Args_new(u32 argc, in char* argv[]) {
    Args args;
    args.argc = argc;
    args.argv = argv;
    args.index = 0;

    return args;
}

char* Args_next(inout Args* self) {
    self->index ++;
    if(self->index == self->argc) {
        return NULL;
    }

    return self->argv[self->index];
}

SResult AsmCmdArgs_parse(Args args, out AsmCmdArgs* asm_cmd_args) {
    static char IMPORT_PATH_DEFAULT[] = ".";
    asm_cmd_args->path = NULL;
    asm_cmd_args->import_path = Vec_new(sizeof(char*));
    char* path_default = IMPORT_PATH_DEFAULT;
    Vec_push(&asm_cmd_args->import_path, &path_default);
    asm_cmd_args->output_path[0] = '\0';
    asm_cmd_args->version = false;

    loop {
        char* arg = Args_next(&args);
        if(arg == NULL) {
            break;
        }
        if(arg[0] != '-') {
            asm_cmd_args->path = arg;
            continue;
        }

        if(strcmp(arg, "-i") == 0) {
            char* import_path = Args_next(&args);
            if(import_path == NULL) {
                AsmCmdArgs_free(*asm_cmd_args);
                return SResult_new("expected import path");
            }
            Vec_push(&asm_cmd_args->import_path, &import_path);
            continue;
        }
        if(strcmp(arg, "-o") == 0) {
            char* output_path = Args_next(&args);
            if(output_path == NULL) {
                AsmCmdArgs_free(*asm_cmd_args);
                return SResult_new("expected output path");
            }
            snprintf(asm_cmd_args->output_path, 256, "%.255s", output_path);
            continue;
        }
        if(strcmp(arg, "-v") == 0) {
            asm_cmd_args->version = true;
            continue;
        }
    }

    return SResult_new(NULL);
}

void AsmCmdArgs_free(AsmCmdArgs self) {
    Vec_free(self.import_path);
}

u32 Cmd_interpreter(int argc, char* argv[]) {
    SResult sresult;
    
    Args args = Args_new(argc, argv);
    AsmCmdArgs asm_cmd_args;
    sresult = AsmCmdArgs_parse(args, &asm_cmd_args);
    if(!SResult_is_success(sresult)) {
        fprintf(stderr, "%s\n", sresult.error);
        return 1;
    }

    if(asm_cmd_args.version) {
        printf("as-macro version alpha\n");
    }

    return Cmd_build_file(asm_cmd_args);
}

static void Cmd_build_file_save(Generator generator, AsmCmdArgs asm_cmd_args) {
    char save_path[256];
    if(asm_cmd_args.output_path[0] != '\0') {
        snprintf(save_path, 256, "%.255s", asm_cmd_args.output_path);
    }else {
        char temp_path[256];
        snprintf(temp_path, 256, "%.255s", asm_cmd_args.path);
        char* extension = strrchr(temp_path, '.');
        if(extension != NULL) {
            *extension = '\0';
        }
        snprintf(save_path, 256, "%.253s.o", temp_path);
    }
    Vec elf_binary = Elf64(&generator);
    Vec_save(elf_binary, save_path);

    Generator_free(generator);
    AsmCmdArgs_free(asm_cmd_args);
}

u32 Cmd_build_file(AsmCmdArgs asm_cmd_args) {
    if(asm_cmd_args.path == NULL) {
        AsmCmdArgs_free(asm_cmd_args);
        return 0;
    }
    
    GlobalSyntaxTree global_syntax_tree = GlobalSyntaxTree_new(Vec_clone(&asm_cmd_args.import_path, NULL));
    if(!SResult_is_success(GlobalSyntaxTree_parse(&global_syntax_tree, asm_cmd_args.path))) {
        AsmCmdArgs_free(asm_cmd_args);
        GlobalSyntaxTree_free(global_syntax_tree);
        return 1;
    }
    Generator generator = GlobalSyntaxTree_build(global_syntax_tree);

    if(Generator_is_error(&generator)) {
        Cmd_print_errors(&generator);
        Generator_free(generator);
        AsmCmdArgs_free(asm_cmd_args);
        return 1;
    }

    Cmd_build_file_save(generator, asm_cmd_args);

    return 0;
}

void Cmd_print_errors(in Generator* generator) {
    for(u32 i=0; i<Vec_len(&generator->errors); i++) {
        Error* error = Vec_index(&generator->errors, i);
        Offset* offset = &error->offset;
        char* msg = error->msg;

        fprintf(stderr, "%s:%d:%d: \x1b[1;31merror:\x1b[0m %s\n",
            offset->path, offset->line, offset->column,
            msg
        );
    }
}



