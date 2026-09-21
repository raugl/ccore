#pragma once
#include "common.h"
#include "slice.h"
#include "arena_allocator.h"

typedef u8 map_metadata;

typedef struct raw_hashmap {
    arena_t* _arena;
    u8*      _data;
    u32      len, capacity, _available;
} raw_hashmap;

typedef struct map_iterator {
    void*         key;
    void*         value;
    map_metadata* _meta;
    u32           _capacity, _remaining;
    u16           _key_sizeof, _value_sizeof;
    bool          _is_first;
} map_iterator;

static bool map_iterator_next(map_iterator* self) {
    assert(self != NULL);
    const u8 MAP_CLAIMED_MASK = 0x80;

    if (self->_remaining == 0) {
        return false;
    }
    while (self->_capacity-- > 0) {
        if (self->_is_first) {
            self->_is_first = false;
        } else {
            self->_meta++;
            self->key = (u8*)self->key + self->_key_sizeof;
            self->value = (u8*)self->value + self->_value_sizeof;
        }
        if (*self->_meta & MAP_CLAIMED_MASK) {
            self->_remaining--;
            return true;
        }
    }
    panic("iterator ran off the hashmap's end while probing for %d remaining entries", self->_remaining);
}

#define HASHMAP_DECL(K, V) HASHMAP_DECL_RAW(K, V, map_##K##_##V)

#define HASHMAP_DECL_RAW(K, V, Self)                                                                \
    typedef struct Self {                                                                           \
        arena_t* _arena;                                                                            \
        u8*      _data;                                                                             \
        u32      len, capacity, _available;                                                         \
    } Self;                                                                                         \
                                                                                                    \
    Self Self##_init(arena_t* arena);                                                               \
                                                                                                    \
    /* Ensures that the hashmap can hold at least `capacity` items.
     * Rounds up the capacity to the nearest power of 2. */                                         \
    bool Self##_reserve(Self* self, usize capacity);                                                \
                                                                                                    \
    /* Ensures that the hashmap can hold at least `count` extra items, on top of the already
     * resident ones. Rounds up the total capacity to the nearest power of 2. */                    \
    bool Self##_reserve_spare(Self* self, usize count);                                             \
                                                                                                    \
    /* Rehash the map, in-place. Due to the tombstone implementation, the hashmap can become slow
     * after many `insert`s and `remove`s. After this function is called, there will be no tombstones
     * in the hashmap, each of the entries is rehashed and any existing pointers or iterators into
     * the hashmap are invalidated. */                                                              \
    void Self##_rehash(Self* self);                                                                 \
                                                                                                    \
    /* Empties the hashmap, the old memory buffer is retained. */                                   \
    void Self##_clear(Self* self);                                                                  \
                                                                                                    \
    /* Checks for the key inside the map. */                                                        \
    bool Self##_contains(Self self, K key);                                                         \
                                                                                                    \
    /* Checks for the key inside the map and writes a pointer to its value if present. */           \
    bool Self##_get(Self self, K key, V** value);                                                   \
                                                                                                    \
    /* Creates a new entry if necessary, otherwise updates the value of the existing key. */        \
    bool Self##_insert(Self* self, K key, V value);                                                 \
                                                                                                    \
    /* Attempts to remove the specified key, returns false if it was not present. */                \
    bool Self##_remove(Self* self, K key);                                                          \
                                                                                                    \
    /* If key exists this function cannot fail. If the map already contains the `key`, then `value`
     * points to it and `existed` is set to true. Otherwise, inserts a new entry with an undefined
     * value and `existed` is set to false. Always fetches a valid pointer if the return value is
     * not false. May fail only while inserting a new entry, `exited` may be NULL. */               \
    bool Self##_fetch_insert(Self* self, K key, V** value, bool* existed);                          \
                                                                                                    \
    /* Writes out the key's value before removing the entry from the map. If the key was not
     * present, returns false and the out pointer is untouched. `value` may not be NULL. */         \
    bool Self##_fetch_remove(Self* self, K key, V* value);                                          \
                                                                                                    \
    map_iterator Self##_iterator(Self self);

#ifdef GENERICS_IMPLEMENTATION
#include "hash.h"
#include "math.h"
#include "slice.h"
#include "testing.h"

#define MAP_FREE_SLOT        0x00
#define MAP_TOMBSTONE_SLOT   0x7F
#define MAP_CLAIMED_MASK     0x80
#define MAP_FINGERPRINT_MASK 0x7F
#define MAP_INVALID_INDEX    UINT32_MAX

#define map_keys(map, K)                                                                            \
    ((K*)align_forward_ptr((map)->_data + sizeof(map_metadata) * (map)->capacity, alignof(K)))

#define map_values(map, K, V)                                                                       \
    ((V*)align_forward_ptr(map_keys((map), K) + (map)->capacity, alignof(V)))


FORCE_INLINE u8 take_hash_fingerprint(u64 hash) {
    return (u8)(hash >> (64 - 7));
}

FORCE_INLINE void mem_swap(void* a, void* b, usize size) {
    assert(a != NULL);
    assert(b != NULL);

    for (usize i = 0; i < size; ++i) {
        u8 tmp = ((u8*)a)[i];
        ((u8*)a)[i] = ((u8*)b)[i];
        ((u8*)b)[i] = tmp;
    }
}

FORCE_INLINE u64 hash_slice(const void* key, u32 size) {
    assert(key != NULL);
    (void)size;

    const slice_raw* slice = key;
    return hash_bytes(slice->data, slice->len);
}

FORCE_INLINE bool equal_slice(const void* a, const void* b, u32 size) {
    assert(a != NULL);
    assert(b != NULL);
    (void)size;
    const slice_raw *slice_a = a, *slice_b = b;

    if (slice_a->len != slice_b->len) return false;
    if (slice_a->len == 0 || slice_a->data == slice_b->data) return true;
    return memcmp(slice_a->data, slice_b->data, slice_a->len) == 0;
}

FORCE_INLINE bool equal_bytes(const void* a, const void* b, u32 size) {
    assert(a != NULL);
    assert(b != NULL);

    if (a == b) return true;
    return memcmp(a, b, size) == 0;
}

FORCE_INLINE bool equal_ptr(const rawptr* a, const rawptr* b, u32 size) {
    assert(a != NULL);
    assert(b != NULL);
    assert(size == sizeof(void*));

    return (*a == *b);
}

#define HASHMAP_IMPL(K, V, hash_proc, equal_proc)                                                   \
    HASHMAP_IMPL_RAW(K, V, map_##K##_##V, hash_proc, equal_proc, 70)

#define HASHMAP_IMPL_RAW(K, V, Self, hash_proc, equal_proc, LOAD_FACTOR)                            \
    Self Self##_init(arena_t* arena) {                                                              \
        assert(arena != NULL);                                                                      \
        return (Self) { ._arena = arena };                                                          \
    }                                                                                               \
                                                                                                    \
    static u32 Self##_probe_index_for_get(Self self, K key) {                                       \
        const u64 hash = hash_proc(&key, sizeof(K));                                                \
        const u32 mask = self.capacity - 1;                                                         \
        const u8 fingerprint = take_hash_fingerprint(hash);                                         \
                                                                                                    \
        K* keys = map_keys(self, K);                                                                \
        u32 index = (u32)hash & mask;                                                               \
        map_metadata* metas = self->_data;                                                          \
                                                                                                    \
        for (u32 limit = 0; limit < self->capacity; ++limit) {                                      \
            if (metas[index] == MAP_FREE_SLOT) {                                                    \
                return MAP_INVALID_INDEX;                                                           \
            }                                                                                       \
            if (metas[index] == (MAP_CLAIMED_MASK | fingerprint)) {                                 \
                if (equal_proc(&key, keys + index, sizeof(K))) return index;                        \
            }                                                                                       \
            index = (index + 1) & mask;                                                             \
        }                                                                                           \
        return MAP_INVALID_INDEX;                                                                   \
    }                                                                                               \
                                                                                                    \
    static u32 Self##_probe_index_for_insert(Self self, K key) {                                    \
        const u64 hash = hash_proc(&key, sizeof(K));                                                \
        const u32 mask = self.capacity - 1;                                                         \
        const u8 fingerprint = take_hash_fingerprint(hash);                                         \
                                                                                                    \
        K* keys = map_keys(self, K);                                                                \
        u32 index = (u32)hash & mask;                                                               \
        map_metadata* metas = self->_data;                                                          \
        u32 tombstone_index = MAP_INVALID_INDEX;                                                    \
                                                                                                    \
        for (u32 limit = 0; limit < self->capacity; ++limit) {                                      \
            if (metas[index] == MAP_FREE_SLOT) {                                                    \
                return (tombstone_index != MAP_INVALID_INDEX) ? tombstone_index : index;            \
            }                                                                                       \
            if (metas[index] == (MAP_CLAIMED_MASK | fingerprint)) {                                 \
                if (equal_proc(&key, keys + index, sizeof(K))) return index;                        \
            }                                                                                       \
            if (tombstone_index == MAP_INVALID_INDEX && metas[index] == MAP_TOMBSTONE_SLOT) {       \
                tombstone_index = index;                                                            \
            }                                                                                       \
            index = (index + 1) & mask;                                                             \
        }                                                                                           \
        return tombstone_index;                                                                     \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve(Self* self, usize new_capacity) {                                           \
        assert(self != NULL);                                                                       \
                                                                                                    \
        new_capacity = max_usize(8, next_pow2_u64(new_capacity * 100 / LOAD_FACTOR));               \
        if (new_capacity <= self->capacity) return true;                                            \
                                                                                                    \
        new_capacity = max_usize(new_capacity, next_pow2_u64(self->capacity));                      \
        if (new_capacity > UINT32_MAX) return false;                                                \
                                                                                                    \
        arena_relocate_info info;                                                                   \
        const u64 new_size = new_capacity * (sizeof(map_metadata) + sizeof(k) + sizeof(V) + 2) - 2; \
        if (!arena_begin_remap(self->_arena, self->_data, self->capacity, new_size, &info)) {       \
            return false;                                                                           \
        }                                                                                           \
        memset(info.ptr, MAP_FREE_SLOT, new_capacity * sizeof(map_metadata));                       \
                                                                                                    \
        Self new_map = {                                                                            \
            ._data      = info.ptr,                                                                 \
            ._arena     = self->_arena,                                                             \
            .capacity   = (u32)new_capacity,                                                        \
            ._available = (u32)(new_capacity * LOAD_FACTOR / 100),                                  \
        };                                                                                          \
        for (u32 i = 0; i < new_capacity; ++i) {                                                    \
            if (!(metas[i] & MAP_CLAIMED_MASK)) continue;                                           \
            assert(new_map._available > 0);                                                         \
                                                                                                    \
            K* keys = map_keys(&new_map, K);                                                        \
            V* vals = map_values(&new_map, K, V);                                                   \
            map_metadata* metas = new_map._data;                                                    \
                                                                                                    \
            const u64 hash = hash_proc(&map_keys(self, K)[i], sizeof(K));                           \
            const u32 mask = new_map.capacity - 1;                                                  \
            const u8 fingerprint = take_hash_fingerprint(hash);                                     \
                                                                                                    \
            u32 index = (u32)hash & mask;                                                           \
            while (metas[index] != MAP_FREE_SLOT) {                                                 \
                index = (index + 1) & mask;                                                         \
            }                                                                                       \
            keys[index] = map_keys(self, K)[i];                                                     \
            vals[index] = map_values(self, K, V)[i];                                                \
            metas[index] = MAP_CLAIMED_MASK | fingerprint;                                          \
                                                                                                    \
            new_map.len++;                                                                          \
            new_map._available--;                                                                   \
            if (self->len == new_map.len) break;                                                    \
        }                                                                                           \
        arena_end_relocate(self->_arena, info);                                                     \
        *self = new_map;                                                                            \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve_spare(Self* self, usize count) {                                            \
        assert(self != NULL);                                                                       \
        return Self##_reserve(self, self->len + count);                                             \
    }                                                                                               \
                                                                                                    \
    void Self##_rehash(Self* self) {                                                                \
        assert(self != NULL);                                                                       \
                                                                                                    \
        K* keys = map_keys(self, K);                                                                \
        V* vals = map_values(self, K, V);                                                           \
        map_metadata* metas = self->_data;                                                          \
                                                                                                    \
        /* While rehashing, use the metadata to mark a slot as either free and available, finalized,
         * or in need of rehashing. The buffer is partitioned by the iteration index, claimed slots
         * ahead of it have found their final position and get the correct fingerprint, and those
         * past it get a temporary fingerprint of all 1s and are to be rehashed at least once more
         * time when the index finally reaches them. */                                             \
        for (u32 i = 0; i < self->capacity; ++i) {                                                  \
            metas[i] &= MAP_CLAIMED_MASK;                                                           \
        }                                                                                           \
                                                                                                    \
        for (u32 curr = 0; curr < self->capacity; ++curr) {                                         \
            if (!(metas[curr] & MAP_CLAIMED_MASK)) {                                                \
                assert(metas[curr] == MAP_FREE_SLOT);                                               \
                continue;                                                                           \
            }                                                                                       \
                                                                                                    \
            const u32 mask = self->capacity - 1;                                                    \
            const u64 hash = hash_proc(&keys[curr], sizeof(K));                                     \
            const u8 fingerprint = take_hash_fingerprint(hash);                                     \
            u32 index = (u32)hash & mask;                                                           \
                                                                                                    \
            while ((index < curr && metas[index] & MAP_CLAIMED_MASK) ||                             \
                (index > curr && metas[index] & MAP_FINGERPRINT_MASK)) {                            \
                index = (index + 1) & mask;                                                         \
            }                                                                                       \
                                                                                                    \
            if (index < 1) {                                                                        \
                assert(metas[index] == MAP_FREE_SLOT);                                              \
                assert(!(metas[curr] & MAP_FINGERPRINT_MASK));                                      \
                                                                                                    \
                metas[index] = MAP_CLAIMED_MASK | fingerprint;                                      \
                keys[index] = keys[curr];                                                           \
                vals[index] = vals[curr];                                                           \
                                                                                                    \
                metas[curr] = MAP_FREE_SLOT;                                                        \
                memset_destroyed(keys + curr, sizeof(K));                                           \
                memset_destroyed(vals + curr, sizeof(V));                                           \
            } else if (index == curr) {                                                             \
                metas[index] = MAP_CLAIMED_MASK | fingerprint;                                      \
            } else {                                                                                \
                assert(!(metas[index] & MAP_FINGERPRINT_MASK));                                     \
                                                                                                    \
                if (metas[index] & MAP_CLAIMED_MASK) {                                              \
                    mem_swap(keys + curr, keys + index, sizeof(K));                                 \
                    mem_swap(vals + curr, vals + index, sizeof(V));                                 \
                    curr--;                                                                         \
                } else {                                                                            \
                    keys[index] = keys[curr];                                                       \
                    vals[index] = vals[curr];                                                       \
                                                                                                    \
                    metas[curr] = MAP_FREE_SLOT;                                                    \
                    memset_destroyed(keys + curr, sizeof(K));                                       \
                    memset_destroyed(vals + curr, sizeof(V));                                       \
                }                                                                                   \
                metas[index] = MAP_CLAIMED_MASK | MAP_FINGERPRINT_MASK;                             \
            }                                                                                       \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    void Self##_clear(Self* self) {                                                                 \
        assert(self != NULL);                                                                       \
        self->len = 0;                                                                              \
        self->_available = (self->capacity * LOAD_FACTOR / 100),                                    \
                                                                                                    \
        memset_destroyed(map_keys(self, K), self->capacity * sizeof(K));                            \
        memset_destroyed(map_values(self, K, V), self->capacity * sizeof(V));                       \
        memset(self->_data, MAP_FREE_SLOT, self->capacity * sizeof(map_metadata));                  \
    }                                                                                               \
                                                                                                    \
    bool Self##_contains(Self self, K key) {                                                        \
        return Self##_probe_index_for_get(self, key) != MAP_INVALID_INDEX;                          \
    }                                                                                               \
                                                                                                    \
    bool Self##_get(Self self, K key, V** value) {                                                  \
        assert(value != NULL);                                                                      \
        const u32 index = Self##_probe_index_for_get(self, key);                                    \
        if (index == MAP_INVALID_INDEX) return false;                                               \
                                                                                                    \
        *value = &map_values(self, K, V)[index];                                                    \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_insert(Self* self, K key, V value) {                                                \
        assert(self != NULL);                                                                       \
        V* value_ptr;                                                                               \
        bool existed; /* This isn't NULL as to train the branch predictor on the more useful case */\
        if (!Self##_fetch_insert(self, key, &value_ptr, &existed)) return false;                    \
        *value_ptr = value;                                                                         \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_fetch_insert(Self* self, K key, V** value, bool* existed) {                         \
        assert(self != NULL);                                                                       \
        assert(value != NULL);                                                                      \
        K* keys = map_keys(self, K);                                                                \
        V* vals = map_values(self, K, V);                                                           \
                                                                                                    \
        if (!Self##_reserve_spare(self, 1)) {                                                       \
            /* If allocation fails, try to do the lookup anyway. If we find an existing item,
             * we can return it. Otherwise return the error, we could not add another. */           \
            const u32 index = Self##_probe_index_for_get(*self, key);                               \
            if (index == MAP_INVALID_INDEX) return false;                                           \
                                                                                                    \
            *value = &vals[index];                                                                  \
            if (existed != NULL) *existed = true;                                                   \
            return true;                                                                            \
        }                                                                                           \
        const u32 index = Self##_probe_index_for_insert(*self, key);                                \
        if (index == MAP_INVALID_INDEX) return false;                                               \
                                                                                                    \
        if (metas[index] != MAP_TOMBSTONE_SLOT) {                                                   \
            *value = &vals[index];                                                                  \
            if (existed != NULL) existed = true;                                                    \
        } else {                                                                                    \
            metas[index] = MAP_CLAIMED_MASK | fingerprint;                                          \
            self->_available--;                                                                     \
            self->len++;                                                                            \
                                                                                                    \
            keys[index] = key;                                                                      \
            *value = &vals[index];                                                                  \
            if (existed != NULL) existed = false;                                                   \
            memset_undefined(vals + index, sizeof(V));                                              \
        }                                                                                           \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_remove(Self* self, K key) {                                                         \
        V value;                                                                                    \
        return Self##_fetch_remove(self, key, &value);                                              \
    }                                                                                               \
                                                                                                    \
    bool Self##_fetch_remove(Self* self, K key, V* value) {                                         \
        assert(self != NULL);                                                                       \
        assert(value != NULL);                                                                      \
                                                                                                    \
        K* keys = map_keys(self, K);                                                                \
        V* vals = map_values(self, K, V);                                                           \
        const u32 index = Self##_probe_index_for_get(*self, key);                                   \
        if (index == MAP_INVALID_INDEX) return false;                                               \
                                                                                                    \
        *value = vals[index];                                                                       \
        memset_destroyed(&keys[index], sizeof(K));                                                  \
        memset_destroyed(&vals[index], sizeof(V));                                                  \
                                                                                                    \
        self->_data[index] = MAP_TOMBSTONE_SLOT;                                                    \
        self->_available++;                                                                         \
        self->len--;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    map_iterator Self##_iterator(Self self) {                                                       \
        return (map_iterator) {                                                                     \
            ._meta         = self._data,                                                            \
            .key           = map_keys(&self, K),                                                    \
            .value         = map_values(&self, K, V),                                               \
            ._is_first     = true,                                                                  \
            ._remaining    = self.len,                                                              \
            ._capacity     = self.capacity,                                                         \
            ._key_sizeof   = sizeof(K),                                                             \
            ._value_sizeof = sizeof(V),                                                             \
        };                                                                                          \
    }
#endif
