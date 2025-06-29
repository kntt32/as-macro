#include <stdio.h>
#include "types.h"

static u64 get_file_size(FILE* stream) {
    if(fseek(stream, 0, SEEK_END)) {
        PANIC("fseek returned error");
    }
    u64 size = ftell(stream);
    if(fseek(stream, 0, SEEK_SET)) {
        PANIC("fseek returned error");
    }

    return size;
}

void Cmd_interpreter(int argc, char* argv[]) {
    // (cmdpath) path
    if(argc != 2) {
        fprintf(stderr, "invalid argument\n");
        return;
    }

    char* path = argv[1];
    FILE* file = fopen(path, "rb");
    if(file == NULL) {
        fprintf(stderr, "file \"%s\" was not found\n", path);
        return;
    }

    printf("filesize: %lu\n", get_file_size(file));
    
    fclose(file);
}

