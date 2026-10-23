#include <assert.h>
#include "hashmap.h"

static u64 hash_string(const char* key) {
    u64 hash = UINT64_C(14695981039346656037);
    for(const unsigned char* p = (const unsigned char*)key; *p; p++) {
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    }
    return hash;
}

int main(void) {
    HashMap map = HashMap_new();
    assert(HashMap_get(&map, "missing").len == 0);

    /* Force collisions in the initial 16-slot table, including wraparound. */
    char keys[10][32];
    u32 found = 0;
    for(u32 i = 0; found < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "collision%u", i);
        if((hash_string(key) & 15) == 15) {
            strcpy(keys[found], key);
            HashMap_add(&map, key, found++);
        }
    }
    for(u32 i = 0; i < 10; i++) {
        Vec indices = HashMap_get(&map, keys[i]);
        assert(indices.len == 1 && *(u32*)Vec_index(&indices, 0) == i);
    }

    /* Stack keys are reused, exercising owned keys and repeated rehashes. */
    for(u32 i = 0; i < 10000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%u", i);
        HashMap_add(&map, key, i);
        HashMap_add(&map, key, i + 10000);
    }
    HashMap_add(&map, "", 42);
    for(u32 i = 0; i < 10000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%u", i);
        Vec indices = HashMap_get(&map, key);
        assert(indices.len == 2);
        assert(*(u32*)Vec_index(&indices, 0) == i);
        assert(*(u32*)Vec_index(&indices, 1) == i + 10000);
    }
    assert(HashMap_get(&map, "missing").len == 0);
    assert(HashMap_get(&map, "").len == 1);
    HashMap_clear_values(&map, keys[0]);
    assert(HashMap_get(&map, keys[0]).len == 0);
    assert(HashMap_get(&map, keys[1]).len == 1);
    HashMap_add(&map, keys[0], 99);
    Vec indices = HashMap_get(&map, keys[0]);
    assert(indices.len == 1 && *(u32*)Vec_index(&indices, 0) == 99);
    HashMap_free(map);
    HashMap_free(HashMap_new());

    Vec vec = Vec_new(sizeof(u32));
    u32 value = 1;
    Vec_push(&vec, &value);
    void* allocation = vec.ptr;
    u32 capacity = vec.capacity;
    for(u32 i = 1; i < capacity; i++) {
        Vec_push(&vec, &i);
        assert(vec.ptr == allocation && vec.capacity == capacity);
    }
    Vec_push(&vec, &value);
    assert(vec.capacity > capacity);
    Vec_free(vec);
    puts("hashmap and vector tests passed");
}
