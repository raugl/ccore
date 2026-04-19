#pragma once
#include "allocator.h"
#include "common.h"

// TODO: Add rehash function, and maybe trigger it automatically when there are too many tombstones.
// TODO: Probably add a clone function at some point.
// TODO: Change it so functions return more information about potential error, especially allocation
// one. Also change the allocators to not panic anymore on failure.

typedef u8 meta_t;

typedef struct {
    void *key, *value;
    bool existed;
} map_entry_t;

typedef struct {
    meta_t* meta;
    void* key;
    void* value;
    u32 capacity, remaining;
    u16 key_sizeof, value_sizeof;
    bool is_first;
} map_iter_t;

#define HASHMAP_RAW_DECL(K, V, name)                                                               \
    typedef struct {                                                                               \
        meta_t* ptr;                                                                               \
        grow_policy_t grow;                                                                        \
        u32 len, capacity, available;                                                              \
    } name##_t;                                                                                    \
                                                                                                   \
    name##_t name##_init_fixed(void* buffer, usize capacity);                                      \
    name##_t name##_init_arena(arena_t* arena, usize capacity);                                    \
    name##_t name##_init_alloc(allocator_t* alloc, usize capacity);                                \
    void name##_release(name##_t* self);                                                           \
                                                                                                   \
    bool name##_contains(name##_t* self, K key);                                                   \
    NULLABLE V* name##_get(name##_t* self, K key);                                                 \
    map_entry_t name##_get_entry(name##_t* self, K key);                                           \
    void name##_put(name##_t* self, K key, V value);                                               \
    V name##_fetch_put(name##_t* self, K key, V value);                                            \
    map_entry_t name##_get_or_put(name##_t* self, K key);                                          \
    void name##_remove(name##_t* self, K key);                                                     \
    map_iter_t name##_iter(name##_t self);

#define HASHMAP_DECL(K, V) HASHMAP_RAW_DECL(K, V, map_##K##_##V)

#define HASHMAP_ARRAY(var, K, V, size)                                                             \
    alignas(max_align_t)                                                                           \
        u8 var[(sizeof(meta_t) + sizeof(K) + sizeof(V)) * size + sizeof(max_align_t) * 2]

// Specialize here whichever hashmaps you always want available. Instantiation is also possible in a
// single translation unit for more specialized types.
HASHMAP_RAW_DECL(u32, u32, map_u32);

#ifdef GENERICS_IMPLEMENTATION
#include "hash.h"
#include "math.h"
#include "slice.h"

// TODO: Try to pull out as much duplicated computation into non-generics helpers

#define INVALID_IDX               UINT_MAX
#define LOAD_FACTOR               80
#define MIN_CAPACITY              8
#define METADATA_FREE             0x0
#define METADATA_TOMBSTONE        0x01
#define METADATA_USED_MASK        0x80
#define METADATA_FINGERPRINT_MASK 0x7f

#define map_keys(map, K)                                                                           \
    (K*)align_forward_ptr((map).ptr + sizeof(meta_t) * (map).capacity, alignof(K))

#define map_values(map, K, V) (V*)align_forward_ptr(map_keys((map), K) + (map).capacity, alignof(V))

static inline bool is_used(u8 meta) {
    return (meta & METADATA_USED_MASK) != 0;
}

static inline u8 set_fingerprint(u8 fingerprint) {
    return fingerprint | METADATA_USED_MASK;
}

static inline u8 get_fingerprint(u8 meta) {
    return meta & METADATA_FINGERPRINT_MASK;
}

static inline u8 take_fingerprint(u64 hash) {
    return hash >> 59;
}

static usize buffer_size(usize capacity, usize key_sizeof, usize value_sizeof) {
    usize size = 0;
    size += capacity * sizeof(meta_t);
    size = align_forward(size, alignof(max_align_t));
    size += capacity * key_sizeof;
    size = align_forward(size, alignof(max_align_t));
    size += capacity * value_sizeof;
    size = align_forward(size, alignof(max_align_t));
    return size;
}

static u8* alloc_buffer(grow_policy_t grow, usize capacity, usize key_sizeof, usize value_sizeof) {
    usize size = buffer_size(capacity, key_sizeof, value_sizeof);
    if (size == 0) return NULL;
    u8* ptr;

    switch (grow.kind) {
    case GROW_POLICY_ARENA:
        ptr = arena_alloc_raw(grow.arena, size * sizeof(u8));
        break;
    case GROW_POLICY_ALLOC:
        ptr = alloc_alloc_raw(grow.alloc, size * sizeof(u8));
        break;
    case GROW_POLICY_FIXED:
        panic("fixed buffer hashmap cannot allocate new buffer");
        break;
    }
    memset(ptr, METADATA_FREE, capacity * sizeof(meta_t));
    return ptr;
}

static inline u64 hash_slice(const void* key, usize size) {
    const slice_raw_t* slice = key;
    (void)size;

    return hash_bytes(slice->ptr, slice->len);
}

static inline bool equal_slice(const void* a, const void* b, usize size) {
    const slice_raw_t *slice_a = a, *slice_b = b;
    (void)size;

    if (slice_a->len != slice_b->len) return false;
    if (slice_a->ptr == slice_b->ptr) return true;
    return memcmp(slice_a->ptr, slice_b->ptr, slice_a->len) == 0;
}

static inline bool equal_bytes(const void* a, const void* b, usize size) {
    if (a == b) return true;
    return memcmp(a, b, size) == 0;
}

bool map_iter_next(map_iter_t* self) {
    assert(self != NULL);

    if (self->remaining == 0) {
        return false;
    }
    while (self->capacity-- > 0) {
        if (self->is_first) {
            self->is_first = false;
        } else {
            self->meta++;
            self->key = (u8*)self->key + self->key_sizeof;
            self->value = (u8*)self->value + self->value_sizeof;
        }
        if (is_used(*self->meta)) {
            self->remaining--;
            return true;
        }
    }
    panic(
        "iterator ran off the hashmap's end while probing for %d remaining entries",
        self->remaining
    );
}

#define HASHMAP_RAW_IMPL(K, V, name, hash_fn, equal_fn)                                            \
    name##_t name##_init_fixed(void* buffer, usize capacity) {                                     \
        assert(buffer != NULL);                                                                    \
        assert(is_pow2(capacity));                                                                 \
        assert(is_aligned_ptr(buffer));                                                            \
                                                                                                   \
        return (name##_t) {                                                                        \
            .ptr = buffer,                                                                         \
            .capacity = capacity,                                                                  \
            .available = (capacity * LOAD_FACTOR / 100),                                           \
            .grow.kind = GROW_POLICY_FIXED,                                                        \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_arena(arena_t* arena, usize capacity) {                                   \
        assert(arena != NULL);                                                                     \
        assert(is_pow2(capacity));                                                                 \
                                                                                                   \
        grow_policy_t grow = {                                                                     \
            .arena = arena,                                                                        \
            .kind = GROW_POLICY_ARENA,                                                             \
        };                                                                                         \
        return (name##_t) {                                                                        \
            .ptr = alloc_buffer(grow, capacity, sizeof(K), sizeof(V)),                             \
            .grow = grow,                                                                          \
            .capacity = capacity,                                                                  \
            .available = (capacity * LOAD_FACTOR / 100),                                           \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_alloc(allocator_t* alloc, usize capacity) {                               \
        assert(alloc != NULL);                                                                     \
        assert(is_pow2(capacity));                                                                 \
                                                                                                   \
        grow_policy_t grow = {                                                                     \
            .alloc = alloc,                                                                        \
            .kind = GROW_POLICY_ALLOC,                                                             \
        };                                                                                         \
        return (name##_t) {                                                                        \
            .ptr = alloc_buffer(grow, capacity, sizeof(K), sizeof(V)),                             \
            .grow = grow,                                                                          \
            .capacity = capacity,                                                                  \
            .available = (capacity * LOAD_FACTOR / 100),                                           \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    void name##_release(name##_t* self) {                                                          \
        assert(self != NULL);                                                                      \
                                                                                                   \
        usize size = buffer_size(self->capacity, sizeof(K), sizeof(V));                            \
        memset(self->ptr, 0, size);                                                                \
                                                                                                   \
        switch (self->grow.kind) {                                                                 \
        case GROW_POLICY_ARENA:                                                                    \
            log_error("attempted to release an arena allocated hashmap");                          \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            alloc_free_raw(self->grow.alloc, self->ptr, size);                                     \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            log_error("attempted to release a fixed buffer hashmap");                              \
            break;                                                                                 \
        }                                                                                          \
        memset(self, 0, sizeof(name##_t));                                                         \
    }                                                                                              \
                                                                                                   \
    static usize name##_get_idx(name##_t* self, K key) {                                           \
        assert(self != NULL);                                                                      \
                                                                                                   \
        K* keys = map_keys(*self, K);                                                              \
                                                                                                   \
        const u64 hash = hash_fn(&key, sizeof(K));                                                 \
        const u32 mask = self->capacity - 1;                                                       \
        const u8 fingerprint = take_fingerprint(hash);                                             \
                                                                                                   \
        u32 idx = hash & mask;                                                                     \
        u32 limit = self->capacity;                                                                \
        meta_t* meta = self->ptr + idx;                                                            \
                                                                                                   \
        while (*meta != METADATA_FREE && limit > 0) {                                              \
            if (is_used(*meta) && get_fingerprint(*meta) == fingerprint) {                         \
                if (equal_fn(&key, keys + idx, sizeof(K))) {                                       \
                    return idx;                                                                    \
                }                                                                                  \
            }                                                                                      \
            limit--;                                                                               \
            idx = (idx + 1) & mask;                                                                \
            meta = self->ptr + idx;                                                                \
        }                                                                                          \
        return INVALID_IDX;                                                                        \
    }                                                                                              \
                                                                                                   \
    NULLABLE V* name##_get(name##_t* self, K key) {                                                \
        usize idx = name##_get_idx(self, key);                                                     \
        if (idx != INVALID_IDX) {                                                                  \
            return map_values(*self, K, V) + idx;                                                  \
        }                                                                                          \
        return NULL;                                                                               \
    }                                                                                              \
                                                                                                   \
    map_entry_t name##_get_entry(name##_t* self, K key) {                                          \
        usize idx = name##_get_idx(self, key);                                                     \
        if (idx != INVALID_IDX) {                                                                  \
            return (map_entry_t) {                                                                 \
                .key = map_keys(*self, K) + idx,                                                   \
                .value = map_values(*self, K, V) + idx,                                            \
                .existed = true,                                                                   \
            };                                                                                     \
        }                                                                                          \
        return (map_entry_t) {.existed = false};                                                   \
    }                                                                                              \
                                                                                                   \
    bool name##_contains(name##_t* self, K key) {                                                  \
        return name##_get_idx(self, key) != INVALID_IDX;                                           \
    }                                                                                              \
                                                                                                   \
    void name##_remove(name##_t* self, K key) {                                                    \
        usize idx = name##_get_idx(self, key);                                                     \
        if (idx == INVALID_IDX) log_error("attempted to remove a non-existent key");               \
                                                                                                   \
        K* keys = map_keys(*self, K);                                                              \
        V* values = map_values(*self, K, V);                                                       \
        memset(keys + idx, 0, sizeof(K));                                                          \
        memset(values + idx, 0, sizeof(V));                                                        \
                                                                                                   \
        self->ptr[idx] = METADATA_TOMBSTONE;                                                       \
        self->available++;                                                                         \
        self->len--;                                                                               \
    }                                                                                              \
                                                                                                   \
    void name##_put(name##_t* self, K key, V value) {                                              \
        map_entry_t gop = name##_get_or_put(self, key);                                            \
        *(V*)gop.value = value;                                                                    \
    }                                                                                              \
                                                                                                   \
    V name##_fetch_put(name##_t* self, K key, V value) {                                           \
        map_entry_t gop = name##_get_or_put(self, key);                                            \
        V result = *(V*)gop.value;                                                                 \
        *(V*)gop.value = value;                                                                    \
        return result;                                                                             \
    }                                                                                              \
                                                                                                   \
    static void name##_put_assume_capacity_no_clobber(name##_t* self, K key, V value) {            \
        const u64 hash = hash_fn(&key, sizeof(K));                                                 \
        const u32 mask = self->capacity - 1;                                                       \
        const u8 fingerprint = take_fingerprint(hash);                                             \
                                                                                                   \
        u32 idx = hash & mask;                                                                     \
        meta_t* meta = self->ptr + idx;                                                            \
                                                                                                   \
        while (is_used(*meta)) {                                                                   \
            idx = (idx + 1) & mask;                                                                \
            meta = self->ptr + idx;                                                                \
        }                                                                                          \
                                                                                                   \
        K* keys = map_keys(*self, K);                                                              \
        V* values = map_values(*self, K, V);                                                       \
        assert(self->available > 0);                                                               \
                                                                                                   \
        *meta = set_fingerprint(fingerprint);                                                      \
        self->available--;                                                                         \
        self->len++;                                                                               \
                                                                                                   \
        keys[idx] = key;                                                                           \
        values[idx] = value;                                                                       \
    }                                                                                              \
                                                                                                   \
    static void name##_grow_at_least(name##_t* self, usize capacity) {                             \
        assert(self != NULL);                                                                      \
        capacity = next_pow2_u64(capacity);                                                        \
                                                                                                   \
        name##_t new_map = {                                                                       \
            .ptr = alloc_buffer(self->grow, capacity, sizeof(K), sizeof(V)),                       \
            .len = 0,                                                                              \
            .grow = self->grow,                                                                    \
            .capacity = capacity,                                                                  \
            .available = (capacity * LOAD_FACTOR) / 100,                                           \
        };                                                                                         \
                                                                                                   \
        if (self->len > 0) {                                                                       \
            K* keys = map_keys(*self, K);                                                          \
            V* values = map_values(*self, K, V);                                                   \
            meta_t* metas = self->ptr;                                                             \
                                                                                                   \
            for (usize i = 0; i < self->capacity; i++) {                                           \
                if (!is_used(metas[i])) continue;                                                  \
                name##_put_assume_capacity_no_clobber(&new_map, keys[i], values[i]);               \
                if (self->len == new_map.len) break;                                               \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        self->len = 0;                                                                             \
        name##_release(self);                                                                      \
        *self = new_map;                                                                           \
    }                                                                                              \
                                                                                                   \
    map_entry_t name##_get_or_put(name##_t* self, K key) {                                         \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->available == 0) {                                                                \
            name##_grow_at_least(self, max(usize, MIN_CAPACITY, self->capacity + 1));              \
        }                                                                                          \
        K* keys = map_keys(*self, K);                                                              \
        V* values = map_values(*self, K, V);                                                       \
                                                                                                   \
        const u64 hash = hash_fn(&key, sizeof(K));                                                 \
        const u32 mask = self->capacity - 1;                                                       \
        const u8 fingerprint = take_fingerprint(hash);                                             \
                                                                                                   \
        u32 idx = hash & mask;                                                                     \
        u32 limit = self->capacity;                                                                \
        u32 first_tombstone_idx = INVALID_IDX;                                                     \
        meta_t* meta = self->ptr + idx;                                                            \
                                                                                                   \
        while (*meta != METADATA_FREE && limit > 0) {                                              \
            if (is_used(*meta) && get_fingerprint(*meta) == fingerprint) {                         \
                K* test_key = keys + idx;                                                          \
                                                                                                   \
                if (equal_fn(&key, test_key, sizeof(K))) {                                         \
                    return (map_entry_t) {                                                         \
                        .key = test_key,                                                           \
                        .value = values + idx,                                                     \
                        .existed = true,                                                           \
                    };                                                                             \
                }                                                                                  \
            } else if (first_tombstone_idx == INVALID_IDX && *meta == METADATA_TOMBSTONE) {        \
                first_tombstone_idx = idx;                                                         \
            }                                                                                      \
            limit--;                                                                               \
            idx = (idx + 1) & mask;                                                                \
            meta = self->ptr + idx;                                                                \
        }                                                                                          \
                                                                                                   \
        if (first_tombstone_idx != INVALID_IDX) {                                                  \
            idx = first_tombstone_idx;                                                             \
            meta = self->ptr + idx;                                                                \
        }                                                                                          \
        *meta = set_fingerprint(fingerprint);                                                      \
        self->available--;                                                                         \
        self->len++;                                                                               \
                                                                                                   \
        keys[idx] = key;                                                                           \
        memset(values + idx, 0, sizeof(V));                                                        \
                                                                                                   \
        return (map_entry_t) {                                                                     \
            .key = keys + idx,                                                                     \
            .value = values + idx,                                                                 \
            .existed = false,                                                                      \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    map_iter_t name##_iter(name##_t self) {                                                        \
        return (map_iter_t) {                                                                      \
            .meta = self.ptr,                                                                      \
            .key = map_keys(self, K),                                                              \
            .value = map_values(self, K, V),                                                       \
            .is_first = true,                                                                      \
                                                                                                   \
            .remaining = self.len,                                                                 \
            .capacity = self.capacity,                                                             \
            .key_sizeof = sizeof(K),                                                               \
            .value_sizeof = sizeof(V),                                                             \
        };                                                                                         \
    }

#define HASHMAP_IMPL(K, V, hash_fn, equal_fn)                                                      \
    HASHMAP_RAW_IMPL(K, V, map_##K##_##V, hash_fn, equal_fn)
#endif
