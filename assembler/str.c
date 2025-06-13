#include <stdio.h>
#include "vec.h"
#include "str.h"

static char null_char = '\0';

String String_new() {
    String string = {Vec_new(sizeof(char))};
    Vec_push(&string.vec, &null_char);
    return string;
}

void String_free(String self) {
    Vec_free(self.vec);
}

char* String_as_ptr(String* self) {
    return Vec_as_ptr(&self->vec);
}

void String_append(String* self, char* src) {
    u32 src_len = strlen(src);
    
    Vec_pop(&self->vec, NULL);
    for(u32 i=0; i<src_len; i++) {
        Vec_push(&self->vec, src + i);
    }
    Vec_push(&self->vec, &null_char);

    return;
}

void String_print(in String* self) {
   for(u32 i=0; i<Vec_len(&self->vec); i++) {
        char* c_ptr = Vec_index(&self->vec, i);
        char c = *c_ptr;
        if(c != '\0') {
            printf("%c", c);
        }
   }
}



