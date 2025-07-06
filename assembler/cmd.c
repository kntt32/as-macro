#include <stdio.h>
#include "types.h"
#include "vec.h"
#include "gen.h"
#include "global_syntax.h"
#include "util.h"
#include "cmd.h"

void Cmd_interpreter(int argc, char* argv[]) {
    // (cmdpath) path

    Vec pathes = Vec_new(sizeof(char*));
    
    for(u32 i=1; i<(u32)argc; i++) {
        if(argv[i][0] == '-') {
            fprintf(stderr, "invalid option");
            goto catch;
        }else {
            Vec_push(&pathes, &argv[i]);
        }
    }

    for(u32 i=0; i<Vec_len(&pathes); i++) {
        char** path_ptr = Vec_index(&pathes, i);
        char* path = *path_ptr;
        FILE* file = fopen(path, "rb");
        if(file == NULL) {
            fprintf(stderr, "file \"%s\" was not found", path);
            goto catch;
        }

        u64 file_size = get_file_size(file);
        char* file_str = malloc(file_size + 1);
        UNWRAP_NULL(file_str);
        if(fread(file_str, 1, file_size, file) <= 0) {
            PANIC("something wrong");
        }
        file_str[file_size] = '\0';
        fclose(file);

        Parser parser = Parser_new(file_str, path);
        
        GlobalSyntaxTree global_syntax_tree = GlobalSyntaxTree_new();
        GlobalSyntaxTree_parse(&global_syntax_tree, parser);
        Generator generator = GlobalSyntaxTree_build((global_syntax_tree));
        printf("%s:\n", path);
        Generator_print(&generator);
        printf("\n\n");
        Generator_free(generator);

        free(file_str);
    }

    Vec_free(pathes);
    return;

catch:
    Vec_free(pathes);
    return;
}



