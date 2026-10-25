#pragma once

#include "vec.h"

typedef struct HashMapEntry HashMapEntry;

/* String -> insertion-ordered vector of indices. Keys and index vectors are
 * owned by the map; indices remain valid when the indexed Vec reallocates. */
typedef struct {
    HashMapEntry* entries;
    u32 len;
    u32 capacity;
} HashMap;

HashMap HashMap_new(void);
void HashMap_free(HashMap self);
void HashMap_add(HashMap* self, const char* key, u32 index);
/* Borrowed view: do not free or mutate it. Invalidated by map mutation. */
Vec HashMap_get(const HashMap* self, const char* key);
void HashMap_clear_values(HashMap* self, const char* key);
