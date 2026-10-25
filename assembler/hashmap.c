#include "hashmap.h"

struct HashMapEntry {
    char* key;
    u64 hash;
    Vec indices;
};

static u64 hash_string(const char* key) {
    u64 hash = UINT64_C(14695981039346656037);
    for(const unsigned char* p = (const unsigned char*)key; *p; p++) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static HashMapEntry* find_entry(const HashMap* self, const char* key, u64 hash) {
    u32 slot = hash & (self->capacity - 1);
    while(self->entries[slot].key != NULL) {
        HashMapEntry* entry = &self->entries[slot];
        if(entry->hash == hash && strcmp(entry->key, key) == 0) {
            return entry;
        }
        slot = (slot + 1) & (self->capacity - 1);
    }
    return &self->entries[slot];
}

HashMap HashMap_new(void) {
    return (HashMap){0};
}

static void grow(HashMap* self) {
    HashMap old = *self;
    if(old.capacity > UINT32_MAX / 2) {
        PANIC("hash map capacity overflow");
    }
    self->capacity = old.capacity ? old.capacity * 2 : 16;
    self->entries = calloc(self->capacity, sizeof(HashMapEntry));
    UNWRAP_NULL(self->entries);
    for(u32 i = 0; i < old.capacity; i++) {
        HashMapEntry* entry = &old.entries[i];
        if(entry->key != NULL) {
            *find_entry(self, entry->key, entry->hash) = *entry;
        }
    }
    free(old.entries);
}

void HashMap_add(HashMap* self, const char* key, u32 index) {
    u64 hash = hash_string(key);
    HashMapEntry* entry = self->capacity ? find_entry(self, key, hash) : NULL;
    if(entry == NULL || entry->key == NULL) {
        /* Keep at least 25% of slots empty to bound average probe length. */
        if(self->len >= self->capacity - self->capacity / 4) {
            grow(self);
        }
        entry = find_entry(self, key, hash);
        entry->key = malloc(strlen(key) + 1);
        UNWRAP_NULL(entry->key);
        strcpy(entry->key, key);
        entry->hash = hash;
        entry->indices = Vec_new(sizeof(u32));
        self->len++;
    }
    Vec_push(&entry->indices, &index);
}

Vec HashMap_get(const HashMap* self, const char* key) {
    if(self->capacity) {
        HashMapEntry* entry = find_entry(self, key, hash_string(key));
        if(entry->key != NULL) {
            return entry->indices;
        }
    }
    return Vec_new(sizeof(u32));
}

void HashMap_clear_values(HashMap* self, const char* key) {
    if(self->capacity) {
        HashMapEntry* entry = find_entry(self, key, hash_string(key));
        if(entry->key != NULL) {
            entry->indices.len = 0;
        }
    }
}

void HashMap_free(HashMap self) {
    for(u32 i = 0; i < self.capacity; i++) {
        if(self.entries[i].key != NULL) {
            free(self.entries[i].key);
            Vec_free(self.entries[i].indices);
        }
    }
    free(self.entries);
}
