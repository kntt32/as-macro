#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "types.h"
#include "util.h"

char* Util_trim_str(in char* str) {
    while(isspace(str[0])) {
        str++;
    }
    return str;
}

static u64* Util_str_to_i64_helper(in char* str, in char* prefix, in u8 base, out u64* ptr) {
    *ptr = 0;
    str = Util_trim_str(str);
    
    for(u32 i=0; i<strlen(prefix); i++) {
        if(str[0] != prefix[i]) {
            return NULL;
        }
        str ++;
    }
    
    while(str[0] != '\0') {
        u8 value = str[0] - '0';
        if(10 <= value) {
            if(value < 'a' - '0') {
                return NULL;
            }
            value += '9' - 'a' + 1;
        }
        if(base <= value) {
            return NULL;
        }
        *ptr = *ptr * base + value;
        str ++;
    }
    
    return ptr;
}

u64* Util_str_to_i64(in char* str, out i64* ptr) {
    static struct {char* prefix; u8 base; } TABLE[] = {{"0b", 2}, {"0o", 8}, {"", 10}, {"0x", 16}};

    str = Util_trim_str(str);
    bool minus_flag = false;
    if(str[0] == '-') {
        minus_flag = true;
        str++;
    }
    for(u32 i=0; i<LEN(TABLE); i++) {
        if(Util_str_to_i64_helper(str, TABLE[i].prefix, TABLE[i].base, ptr) != NULL) {
            if(minus_flag) {
                *ptr = - (*ptr);
            }
            return ptr;
        }
    }
    
    return NULL;
}

static bool Util_is_number_helper(in char* str, in char* prefix, in u8 base) {
    str = Util_trim_str(str);

    if(strlen(str) == 0) {
        return false;
    }
    
    for(u32 i=0; i<strlen(prefix); i++) {
        if(str[0] != prefix[i]) {
            return false;
        }
        str ++;
    }
    
    while(str[0] != '\0') {
        u8 value = str[0] - '0';
        if(10 <= value) {
            if(value < 'a' - '0') {
                return false;
            }
            value += '9' - 'a' + 1;
        }
        if(base <= value) {
            return false;
        }
        str ++;
    }
    
    return true;
}


bool Util_is_number(in char* str) {
    str = Util_trim_str(str);
    if(str[0] == '-' || str[0] == '+') {
        str ++;
    }

    return Util_is_number_helper(str, "0b", 2)
        || Util_is_number_helper(str, "0o", 8)
        || Util_is_number_helper(str, "", 10)
        || Util_is_number_helper(str, "0x", 16);
}





