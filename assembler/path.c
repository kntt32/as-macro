#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "types.h"
#include "path.h"

char* path_real(in char* path, out char buffer[4096]) {
    buffer[0] = '\0';

    if(path[0] != '/') {
        if(getcwd(buffer, 4096) == NULL) {
            return NULL;
        }
    }

    while(path[0] != '\0') {
        char* part_start = path;
        char* part_end = strchr(path, '/');
        u64 len;
        if(part_end == NULL) {
            len = strlen(part_start);
        }else {
            len = ((u64)part_end - (u64)part_start)/sizeof(char);
        }
        
        if(len == 2 && strncmp(part_start, "..", 2) == 0) {
            if(path_super(buffer) == NULL) {
                return NULL;
            }
        }else if(len == 1 && strncmp(part_start, ".", 1) == 0) {
        }else if(len == 0) {
        }else {
            if(4096 <= strlen(buffer) + 1 + len + 1) {
                return NULL;
            }
            u32 str_len = strlen(buffer);
            strcpy(buffer + str_len, "/");
            memcpy(buffer + str_len + 1, part_start, len);
            buffer[str_len + 1 + len] = '\0';
        }

        path += len;
        if(path[0] != '\0') {
            path ++;
        }
    }

    return buffer;
}

char* path_super(inout char path[4096]) {
    char* super_end = strrchr(path, '/');
    if(super_end == NULL) {
        return NULL;
    }
    super_end[0] = '\0';

    return path;
}

char* path_append_super(inout char path[4096]) {
    if(4096 <= strlen(path) + 3 + 1) {
        return NULL;
    }

    strcpy(path + strlen(path), "/..");
    path[strlen(path)] = '\0';

    return path;
}

char* path_child(inout char path[4096], in char* child) {
    if(4096 <= strlen(path) + 1 + strlen(child) + 1) {
        return NULL;
    }
    strcpy(path + strlen(path), "/");
    strcpy(path + strlen(path), child);

    return path;
}

char* path_append_extension(inout char path[4096], in char* extension) {
    if(4096 <= strlen(path) + 1 + strlen(extension) + 1) {
        return NULL;
    }
    strcpy(path + strlen(path), ".");
    strcpy(path + strlen(path), extension);

    return path;
}

