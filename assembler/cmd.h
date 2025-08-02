#pragma once
#include "types.h"
#include "vec.h"
#include "gen.h"

typedef struct {
    u32 argc;
    char** argv;
    u32 index;
} Args;

typedef struct {
    char* path;
    Vec import_path;// Vec<char*>
    char output_path[256];
    bool version;
} AsmCmdArgs;

Args Args_new(u32 argc, in char* argv[]);
char* Args_next(inout Args* self);

SResult AsmCmdArgs_parse(Args args, out AsmCmdArgs* asm_cmd_args);
void AsmCmdArgs_free(AsmCmdArgs self);

u32 Cmd_interpreter(int argc, char* argv[]);
u32 Cmd_build_file(AsmCmdArgs asm_cmd_args);
void Cmd_print_errors(in Generator* generator);

