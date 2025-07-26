#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "vec.h"

Vec Vec_new(u32 size) {
    Vec vec = {NULL, size, 0, 0};
    return vec;
}

void Vec_free(Vec vec) {
    free(vec.ptr);
}

void Vec_free_all(Vec self, optional in void (*destructor)(void*)) {
    if(destructor != NULL) {
        for(u32 i=0; i<self.len; i++) {
            destructor(Vec_index(&self, i));
        }
    }

    Vec_free(self);

    return;
}

Vec Vec_clone(in Vec* self, optional in void (*clone)(void* dst, void* src)) {
    Vec vec;
    vec.ptr = malloc(self->capacity * self->size);
    vec.size = self->size;
    vec.len = self->len;
    vec.capacity = self->capacity;

    if(clone == NULL) {
        if(vec.len != 0) {
            memcpy(vec.ptr, self->ptr, vec.len * vec.size);
        }
    }else {
        for(u32 i=0; i<vec.len; i++) {
            void* ptr = (u8*)vec.ptr + vec.size * i;
            clone(ptr, Vec_index(self, i));
        }
    }

    return vec;
}

void* Vec_index(Vec* self, u32 index) {
    if(self->len <= index) {
        PANIC("out of range");
    }
    return (u8*)self->ptr + self->size * index;
}

void* Vec_as_ptr(Vec* self) {
    return self->ptr;
}

void Vec_print(Vec* self, void (*f)(void*)) {
    printf("Vec [");
    for(u32 i=0; i<self->len; i++) {
        f(Vec_index(self, i));
        printf(", ");
    }
    printf("]");
}

static inline void Vec_reserve(Vec* self, u32 capacity) {
    if(capacity < self->len) {
        return;
    }
    u32 new_capacity = (self->len == 0)?(4):(self->len * 1.5);
    if(new_capacity < capacity) {
        new_capacity = capacity;
    }
    void* ptr = realloc(self->ptr, new_capacity * self->size);
    UNWRAP_NULL(ptr);

    self->ptr = ptr;
    self->capacity = new_capacity;
}

void Vec_push(Vec* self, void* restrict object) {
    Vec_reserve(self, self->len + 1);
    memcpy((u8*)self->ptr + self->size * self->len, object, self->size);
    self->len ++;
}

void Vec_pop(Vec* self, void* restrict ptr) {
    if(ptr != NULL) {
        Vec_last(self, ptr);
    }
    self->len --;
}

void Vec_insert(Vec* self, u32 index, void* object) {
    Vec_reserve(self, self->len + 1);
    void* dst = (u8*)self->ptr + self->size*(index + 1);
    void* src = (u8*)self->ptr + self->size*index;
    memmove(dst, src, (self->len - index)*self->size);
    self->len ++;
    memcpy((u8*)self->ptr + self->size * index, object, self->size);
}

void Vec_last(Vec* self, void* restrict ptr) {
    if(self->len == 0) {
        PANIC("length is zero");
    } else {
        memcpy(ptr, (u8*)self->ptr + self->size * (self->len - 1), self->size);
    }
}

SResult Vec_last_ptr(Vec* self, void** ptr) {
    if(self->len == 0) {
        return SResult_new("vec length is zero");
    }
    *ptr = (u8*)self->ptr + self->size * (self->len - 1);
    return SResult_new(NULL);
}

Vec Vec_from(void* src, u32 len, u32 size) {
    Vec vec = Vec_new(size);
    Vec_append(&vec, src, len);
    return vec;
}

void Vec_append(Vec* self, void* src, u32 len) {
    Vec_reserve(self, self->len + len);

    memcpy((u8*)self->ptr + self->size * self->len, src, self->size * len);
    self->len += len;
   
    return;
}

void Vec_remove(inout Vec* self, u32 index, out void* ptr) {
    void* ptr_src = Vec_index(self, index);
    memcpy(ptr, ptr_src, self->size);

    if(self->len - index - 1 != 0) {
        memmove(ptr_src, (u8*)ptr_src+self->size, self->size*(self->len-index-1));
    }
    self->len --;
}

u32 Vec_len(Vec* self) {
    return self->len;
}

u32 Vec_capacity(Vec* self) {
    return self->capacity;
}

u32 Vec_size(Vec* self) {
    return self->size;
}

bool Vec_cmp(in Vec* self, in Vec* other, bool (in *cmp)(in void*, in void*)) {
    if(Vec_len(self) != Vec_len(other)) {
        return false;
    }

    for(u32 i=0; i<Vec_len(self); i++) {
        if(!cmp(Vec_index(self, i), Vec_index(other, i))) {
            return false;
        }
    }

    return true;
}

void Vec_save(Vec self, in char* path) {
    if(Vec_size(&self) != sizeof(u8)) {
        PANIC("Vec_size is not 1 byte");
    }
    
    FILE* file = fopen(path, "wb");
    if(file == NULL) {
        PANIC("failed to save");
    }

    fwrite(Vec_as_ptr(&self), sizeof(u8), Vec_len(&self), file);
    fclose(file);

    Vec_free(self);
}

