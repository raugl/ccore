#pragma once
#include "allocator.h"
#include "common.h"

// TODO: Add rehash function, and maybe trigger it automatically when there are too many tombstones.
// TODO: Probably add a clone function at some point.
// TODO: `get_or_put` returns an invalid entry structure, which is used to signify allocation
// failure. That's pretty bad, maybe change to an out param.

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

#define HASHMAP_DECL_RAW(K, V, name)                                                               \
    typedef struct {                                                                               \
        meta_t* ptr_;                                                                              \
        grow_policy_t grow_;                                                                       \
        u32 len, capacity, available;                                                              \
    } name##_t;                                                                                    \
                                                                                                   \
    name##_t name##_init_fixed(void* buffer, usize capacity);                                      \
    name##_t name##_init_arena(arena_t* arena, usize capacity);                                    \
    name##_t name##_init_alloc(allocator_t* alloc, usize capacity);                                \
    void name##_release(name##_t* self);                                                           \
                                                                                                   \
    bool name##_ensure_capacity(name##_t* self, usize new_capacity);                               \
    bool name##_contains(name##_t* self, K key);                                                   \
    NULLABLE V* name##_get(name##_t* self, K key);                                                 \
    map_entry_t name##_get_entry(name##_t* self, K key);                                           \
    bool name##_put(name##_t* self, K key, V value);                                               \
    bool name##_fetch_put(name##_t* self, K key, V value, V* out);                                 \
    map_entry_t name##_get_or_put(name##_t* self, K key);                                          \
    bool name##_remove(name##_t* self, K key);                                                     \
    map_iter_t name##_iter(name##_t self);

#define HASHMAP_DECL(K, V) HASHMAP_DECL_RAW(K, V, map_##K##_##V)

#define HASHMAP_ARRAY(var, K, V, size)                                                             \
    alignas(max_align_t)                                                                           \
        u8 var[(sizeof(meta_t) + sizeof(K) + sizeof(V)) * size + sizeof(max_align_t) * 2]

// Specialize here whichever hashmaps you always want available. Instantiation is also possible in a
// single translation unit for more specialized types.
HASHMAP_DECL_RAW(u32, u32, map_u32);

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
    (K*)align_forward_ptr((map).ptr_ + sizeof(meta_t) * (map).capacity, alignof(K))

#define map_values(map, K, V) (V*)align_forward_ptr(map_keys((map), K) + (map).capacity, alignof(V))

static bool is_used(u8 meta) {
    return (meta & METADATA_USED_MASK) != 0;
}

static u8 set_fingerprint(u8 fingerprint) {
    return fingerprint | METADATA_USED_MASK;
}

static u8 get_fingerprint(u8 meta) {
    return meta & METADATA_FINGERPRINT_MASK;
}

static u8 take_fingerprint(u64 hash) {
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

static u8* alloc_buffer(grow_policy_t grow_, usize capacity, usize key_sizeof, usize value_sizeof) {
    usize size = buffer_size(capacity, key_sizeof, value_sizeof);
    if (size == 0) return NULL;
    u8* ptr;

    switch (grow_.kind) {
    case GROW_POLICY_ARENA:
        ptr = arena_alloc(grow_.arena, size * sizeof(u8));
        break;
    case GROW_POLICY_ALLOC:
        ptr = alloc_alloc(grow_.alloc, size * sizeof(u8));
        break;
    case GROW_POLICY_FIXED:
        panic("fixed buffer hashmap cannot allocate new buffer");
        break;
    }
    // FIXME: potentially writing to NULL
    memset(ptr, METADATA_FREE, capacity * sizeof(meta_t));
    return ptr;
}

static u64 hash_slice(const void* key, usize size) {
    const slice_raw_t* slice = key;
    (void)size;

    return hash_bytes(slice->ptr, slice->len);
}

static bool equal_slice(const void* a, const void* b, usize size) {
    const slice_raw_t *slice_a = a, *slice_b = b;
    (void)size;

    if (slice_a->len != slice_b->len) return false;
    if (slice_a->ptr == slice_b->ptr) return true;
    return memcmp(slice_a->ptr, slice_b->ptr, slice_a->len) == 0;
}

static bool equal_bytes(const void* a, const void* b, usize size) {
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

#define HASHMAP_IMPL_RAW(K, V, name, hash_fn, equal_fn)                                            \
    name##_t name##_init_fixed(void* buffer, usize capacity) {                                     \
        assert(buffer != NULL);                                                                    \
        assert(is_pow2(capacity));                                                                 \
        assert(is_aligned_ptr(buffer));                                                            \
                                                                                                   \
        return (name##_t) {                                                                        \
            .ptr_ = buffer,                                                                        \
            .capacity = capacity,                                                                  \
            .available = (capacity * LOAD_FACTOR / 100),                                           \
            .grow_.kind = GROW_POLICY_FIXED,                                                       \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_arena(arena_t* arena, usize capacity) {                                   \
        assert(arena != NULL);                                                                     \
        if (capacity > 0) {                                                                        \
            capacity = next_pow2_u64(max_usize(MIN_CAPACITY, capacity));                           \
        }                                                                                          \
        usize size = buffer_size(capacity, sizeof(K), sizeof(V));                                  \
        u8* ptr_ = arena_alloc(arena, size * sizeof(u8));                                          \
        assert(ptr_ != NULL);                                                                      \
        memset(ptr_, METADATA_FREE, capacity * sizeof(meta_t));                                    \
                                                                                                   \
        return (name##_t) {                                                                        \
            .ptr_ = ptr_,                                                                          \
            .capacity = capacity,                                                                  \
            .available = (capacity * LOAD_FACTOR / 100),                                           \
            .grow_.arena = arena,                                                                  \
            .grow_.kind = GROW_POLICY_ARENA,                                                       \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_alloc(allocator_t* alloc, usize capacity) {                               \
        assert(alloc != NULL);                                                                     \
        if (capacity > 0) {                                                                        \
            capacity = next_pow2_u64(max_usize(MIN_CAPACITY, capacity));                           \
        }                                                                                          \
        usize size = buffer_size(capacity, sizeof(K), sizeof(V));                                  \
        u8* ptr_ = alloc_alloc(alloc, size * sizeof(u8));                                          \
        assert(ptr_ != NULL);                                                                      \
        memset(ptr_, METADATA_FREE, capacity * sizeof(meta_t));                                    \
                                                                                                   \
        return (name##_t) {                                                                        \
            .ptr_ = ptr_,                                                                          \
            .capacity = capacity,                                                                  \
            .available = (capacity * LOAD_FACTOR / 100),                                           \
            .grow_.alloc = alloc,                                                                  \
            .grow_.kind = GROW_POLICY_ALLOC,                                                       \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    void name##_release(name##_t* self) {                                                          \
        assert(self != NULL);                                                                      \
                                                                                                   \
        usize size = buffer_size(self->capacity, sizeof(K), sizeof(V));                            \
        memset(self->ptr_, 0xDE, size);                                                            \
                                                                                                   \
        switch (self->grow_.kind) {                                                                \
        case GROW_POLICY_ARENA:                                                                    \
            log_error("attempted to release an arena allocated hashmap");                          \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            alloc_free_raw(self->grow_.alloc, self->ptr_, size);                                   \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            log_error("attempted to release a fixed buffer hashmap");                              \
            break;                                                                                 \
        }                                                                                          \
        memset(self, 0xDE, sizeof(name##_t));                                                      \
    }                                                                                              \
                                                                                                   \
    static void name##_put_assume_capacity_no_clobber(name##_t* self, K key, V value) {            \
        const u64 hash = hash_fn(&key, sizeof(K));                                                 \
        const u32 mask = self->capacity - 1;                                                       \
        const u8 fingerprint = take_fingerprint(hash);                                             \
                                                                                                   \
        u32 idx = hash & mask;                                                                     \
        meta_t* meta = self->ptr_ + idx;                                                           \
                                                                                                   \
        while (is_used(*meta)) {                                                                   \
            idx = (idx + 1) & mask;                                                                \
            meta = self->ptr_ + idx;                                                               \
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
    bool name##_ensure_capacity(name##_t* self, usize new_capacity) {                              \
        assert(self != NULL);                                                                      \
        if (self->capacity > new_capacity) return true;                                            \
        new_capacity = next_pow2_u64(max_usize(MIN_CAPACITY, new_capacity));                       \
        name##_t new_map;                                                                          \
                                                                                                   \
        switch (self->grow_.kind) {                                                                \
        case GROW_POLICY_ARENA:                                                                    \
            /* TODO: Return `false` on allocation failure */                                       \
            new_map = name##_init_arena(self->grow_.arena, new_capacity);                          \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            /* TODO: Return `false` on allocation failure */                                       \
            new_map = name##_init_alloc(self->grow_.alloc, new_capacity);                          \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            panic("fixed buffer hashmap cannot allocate new buffer");                              \
            break;                                                                                 \
        }                                                                                          \
        if (self->len > 0) {                                                                       \
            K* keys = map_keys(*self, K);                                                          \
            V* values = map_values(*self, K, V);                                                   \
            meta_t* metas = self->ptr_;                                                            \
                                                                                                   \
            for (usize i = 0; i < self->capacity; i++) {                                           \
                if (!is_used(metas[i])) continue;                                                  \
                name##_put_assume_capacity_no_clobber(&new_map, keys[i], values[i]);               \
                if (self->len == new_map.len) break;                                               \
            }                                                                                      \
        }                                                                                          \
        self->len = 0;                                                                             \
        name##_release(self);                                                                      \
        *self = new_map;                                                                           \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    static usize name##_get_idx(name##_t* self, K key) {                                           \
        assert(self != NULL);                                                                      \
        K* keys = map_keys(*self, K);                                                              \
                                                                                                   \
        const u64 hash = hash_fn(&key, sizeof(K));                                                 \
        const u32 mask = self->capacity - 1;                                                       \
        const u8 fingerprint = take_fingerprint(hash);                                             \
                                                                                                   \
        u32 idx = hash & mask;                                                                     \
        u32 limit = self->capacity;                                                                \
        meta_t* meta = self->ptr_ + idx;                                                           \
                                                                                                   \
        while (*meta != METADATA_FREE && limit > 0) {                                              \
            if (is_used(*meta) && get_fingerprint(*meta) == fingerprint) {                         \
                if (equal_fn(&key, keys + idx, sizeof(K))) {                                       \
                    return idx;                                                                    \
                }                                                                                  \
            }                                                                                      \
            limit--;                                                                               \
            idx = (idx + 1) & mask;                                                                \
            meta = self->ptr_ + idx;                                                               \
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
        return (map_entry_t) {};                                                                   \
    }                                                                                              \
                                                                                                   \
    bool name##_contains(name##_t* self, K key) {                                                  \
        return name##_get_idx(self, key) != INVALID_IDX;                                           \
    }                                                                                              \
                                                                                                   \
    bool name##_remove(name##_t* self, K key) {                                                    \
        usize idx = name##_get_idx(self, key);                                                     \
        if (idx == INVALID_IDX) return false;                                                      \
                                                                                                   \
        K* keys = map_keys(*self, K);                                                              \
        V* values = map_values(*self, K, V);                                                       \
        memset(keys + idx, 0xDE, sizeof(K));                                                       \
        memset(values + idx, 0xDE, sizeof(V));                                                     \
                                                                                                   \
        self->ptr_[idx] = METADATA_TOMBSTONE;                                                      \
        self->available++;                                                                         \
        self->len--;                                                                               \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_put(name##_t* self, K key, V value) {                                              \
        map_entry_t gop = name##_get_or_put(self, key);                                            \
        if (gop.value == NULL) return false;                                                       \
        *(V*)gop.value = value;                                                                    \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_fetch_put(name##_t* self, K key, V value, V* out) {                                \
        map_entry_t gop = name##_get_or_put(self, key);                                            \
        if (gop.value == NULL) return false;                                                       \
        *out = *(V*)gop.value;                                                                     \
        *(V*)gop.value = value;                                                                    \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    map_entry_t name##_get_or_put(name##_t* self, K key) {                                         \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->available == 0) {                                                                \
            /* TODO: try to get only instead, if allocation fails */                               \
            if (!name##_ensure_capacity(self, self->capacity + 1)) return (map_entry_t) {};        \
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
        meta_t* meta = self->ptr_ + idx;                                                           \
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
            meta = self->ptr_ + idx;                                                               \
        }                                                                                          \
                                                                                                   \
        if (first_tombstone_idx != INVALID_IDX) {                                                  \
            idx = first_tombstone_idx;                                                             \
            meta = self->ptr_ + idx;                                                               \
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
            .meta = self.ptr_,                                                                     \
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
    HASHMAP_IMPL_RAW(K, V, map_##K##_##V, hash_fn, equal_fn)
#endif
