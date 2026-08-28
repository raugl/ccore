#pragma once
#include "allocator.h"
#include "common.h"
#include "slice.h"

// TODO: Add rehash function.
// TODO: Probably add a clone function at some point.
// TODO: `get_or_put` returns an invalid entry structure, which is used to signify allocation
// failure. That's pretty bad, maybe change to an out param.

typedef u8 map_meta_t;

typedef struct {
    void *key, *value;
    bool existed;
} map_entry_t;

typedef struct {
    map_meta_t* meta;
    void* key;
    void* value;
    u32 capacity, remaining;
    u16 key_sizeof, value_sizeof;
    bool is_first;
} map_iter_t;

bool map_iter_next(map_iter_t* self);

#define HASHMAP_ARRAY(var, K, V, size)                                                              \
    alignas(max_align_t)                                                                            \
        u8 var[(sizeof(map_meta_t) + sizeof(K) + sizeof(V)) * size + sizeof(max_align_t) * 2]

#define HASHMAP_DECL(K, V) HASHMAP_DECL_RAW(K, V, map_##K##_##V)

#define HASHMAP_DECL_RAW(K, V, Self)                                                                \
    typedef struct {                                                                                \
        map_meta_t* ptr_;                                                                           \
        allocator_t allocator;                                                                      \
        u32 len, capacity, available;                                                               \
    } Self;                                                                                         \
                                                                                                    \
    Self Self##_init_fixed(void* buffer, usize capacity);                                           \
    Self Self##_init_alloc(allocator_t allocator);                                                  \
    void Self##_release(Self* self);                                                                \
                                                                                                    \
    bool Self##_ensure_capacity(Self* self, usize new_capacity);                                    \
    bool Self##_contains(Self* self, K key);                                                        \
    NULLABLE V* Self##_get(Self* self, K key);                                                      \
    map_entry_t Self##_get_entry(Self* self, K key);                                                \
    bool Self##_put(Self* self, K key, V value);                                                    \
    bool Self##_fetch_put(Self* self, K key, V value, V* out);                                      \
    map_entry_t Self##_get_or_put(Self* self, K key);                                               \
    bool Self##_remove(Self* self, K key);                                                          \
    map_iter_t Self##_iter(Self self);

HASHMAP_DECL_RAW(u32, u32, map_u32)
HASHMAP_DECL_RAW(string, u64, map_str_u64)

#ifdef GENERICS_IMPLEMENTATION
#include "hash.h"
#include "math.h"
#include "slice.h"

#define INVALID_IDX               UINT_MAX
#define LOAD_FACTOR               70
#define MIN_CAPACITY              8
#define METADATA_FREE             0x0
#define METADATA_TOMBSTONE        0x01
#define METADATA_USED_MASK        0x80
#define METADATA_FINGERPRINT_MASK 0x7F

#define map_keys(map, K)                                                                            \
    (K*)align_forward_ptr((map).ptr_ + sizeof(map_meta_t) * (map).capacity, alignof(K))

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
    return (u8)(hash >> 59);
}

static u64 hash_slice(const void* key, usize size) {
    const slice_raw* slice = key;
    (void)size;

    return hash_bytes(slice->ptr, slice->len);
}

static bool equal_slice(const void* a, const void* b, usize size) {
    const slice_raw *slice_a = a, *slice_b = b;
    (void)size;

    if (slice_a->len != slice_b->len) return false;
    if (slice_a->len == 0 || slice_a->ptr == slice_b->ptr) return true;
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

#define HASHMAP_IMPL(K, V, hash_fn, equal_fn)                                                       \
    HASHMAP_IMPL_RAW(K, V, map_##K##_##V, hash_fn, equal_fn)

#define HASHMAP_IMPL_RAW(K, V, Self, hash_fn, equal_fn)                                             \
    Self Self##_init_fixed(void* buffer, usize capacity) {                                          \
        assert(buffer != NULL);                                                                     \
        assert(is_pow2(capacity));                                                                  \
        assert(is_aligned2_ptr(buffer, max_usize(alignof(K), alignof(V))));                         \
        memset(buffer, METADATA_FREE, capacity * sizeof(map_meta_t));                               \
                                                                                                    \
        return (Self) {                                                                             \
            .ptr_ = buffer,                                                                         \
            .capacity = (u32)capacity,                                                              \
            .available = (u32)(capacity * LOAD_FACTOR / 100),                                       \
            .allocator = allocator_init_null(),                                                     \
        };                                                                                          \
    }                                                                                               \
                                                                                                    \
    Self Self##_init_alloc(allocator_t allocator) {                                                 \
        assert(allocator.proc != NULL);                                                             \
        return (Self) { .allocator = allocator };                                                   \
    }                                                                                               \
                                                                                                    \
    void Self##_release(Self* self) {                                                               \
        assert(self != NULL);                                                                       \
                                                                                                    \
        usize size = 0;                                                                             \
        size += self->capacity * sizeof(map_meta_t);                                                \
        size = align_forward(size, alignof(K));                                                     \
        size += self->capacity * sizeof(K);                                                         \
        size = align_forward(size, alignof(V));                                                     \
        size += self->capacity * sizeof(V);                                                         \
                                                                                                    \
        mem_free(self->allocator, (u8*)self->ptr_, size);                                           \
        memset(self, 0xDE, sizeof(Self));                                                           \
    }                                                                                               \
                                                                                                    \
    static void Self##_put_assume_capacity_no_clobber(Self* self, K key, V value) {                 \
        const u64 hash = hash_fn(&key, sizeof(K));                                                  \
        const u32 mask = self->capacity - 1;                                                        \
        const u8 fingerprint = take_fingerprint(hash);                                              \
                                                                                                    \
        u32 idx = (u32)hash & mask;                                                                 \
        map_meta_t* meta = self->ptr_ + idx;                                                        \
                                                                                                    \
        while (is_used(*meta)) {                                                                    \
            idx = (idx + 1) & mask;                                                                 \
            meta = self->ptr_ + idx;                                                                \
        }                                                                                           \
                                                                                                    \
        K* keys = map_keys(*self, K);                                                               \
        V* values = map_values(*self, K, V);                                                        \
        assert(self->available > 0);                                                                \
                                                                                                    \
        *meta = set_fingerprint(fingerprint);                                                       \
        self->available--;                                                                          \
        self->len++;                                                                                \
                                                                                                    \
        keys[idx] = key;                                                                            \
        values[idx] = value;                                                                        \
    }                                                                                               \
                                                                                                    \
    bool Self##_ensure_capacity(Self* self, usize new_capacity) {                                   \
        assert(self != NULL);                                                                       \
        new_capacity = next_pow2_u64(max_usize(MIN_CAPACITY, new_capacity));                        \
        if (self->capacity > new_capacity) return true;                                             \
                                                                                                    \
        usize size = 0;                                                                             \
        size += self->capacity * sizeof(map_meta_t);                                                \
        size = align_forward(size, alignof(K));                                                     \
        size += self->capacity * sizeof(K);                                                         \
        size = align_forward(size, alignof(V));                                                     \
        size += self->capacity * sizeof(V);                                                         \
        usize align = max_usize(alignof(K), alignof(V));                                            \
                                                                                                    \
        u8* new_ptr = mem_alloc_aligned(self->allocator, u8, size, align);                          \
        if (new_ptr == NULL) return false;                                                          \
        memset(new_ptr, METADATA_FREE, new_capacity * sizeof(map_meta_t));                          \
                                                                                                    \
        Self new_map = {                                                                            \
            .ptr_ = new_ptr,                                                                        \
            .capacity = (u32)new_capacity,                                                          \
            .available = (u32)(new_capacity * LOAD_FACTOR / 100),                                   \
            .allocator = self->allocator,                                                           \
        };                                                                                          \
        if (self->len > 0) {                                                                        \
            K* keys = map_keys(*self, K);                                                           \
            V* values = map_values(*self, K, V);                                                    \
            map_meta_t* metas = self->ptr_;                                                         \
                                                                                                    \
            for (usize i = 0; i < self->capacity; i++) {                                            \
                if (!is_used(metas[i])) continue;                                                   \
                Self##_put_assume_capacity_no_clobber(&new_map, keys[i], values[i]);                \
                if (self->len == new_map.len) break;                                                \
            }                                                                                       \
        }                                                                                           \
        Self##_release(self);                                                                       \
        *self = new_map;                                                                            \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    static usize Self##_get_idx(Self* self, K key) {                                                \
        assert(self != NULL);                                                                       \
        K* keys = map_keys(*self, K);                                                               \
                                                                                                    \
        const u64 hash = hash_fn(&key, sizeof(K));                                                  \
        const u32 mask = self->capacity - 1;                                                        \
        const u8 fingerprint = take_fingerprint(hash);                                              \
                                                                                                    \
        u32 idx = (u32)hash & mask;                                                                 \
        u32 limit = self->capacity;                                                                 \
        map_meta_t* meta = self->ptr_ + idx;                                                        \
                                                                                                    \
        while (*meta != METADATA_FREE && limit > 0) {                                               \
            if (is_used(*meta) && get_fingerprint(*meta) == fingerprint) {                          \
                if (equal_fn(&key, keys + idx, sizeof(K))) {                                        \
                    return idx;                                                                     \
                }                                                                                   \
            }                                                                                       \
            limit--;                                                                                \
            idx = (idx + 1) & mask;                                                                 \
            meta = self->ptr_ + idx;                                                                \
        }                                                                                           \
        return INVALID_IDX;                                                                         \
    }                                                                                               \
                                                                                                    \
    NULLABLE V* Self##_get(Self* self, K key) {                                                     \
        usize idx = Self##_get_idx(self, key);                                                      \
        if (idx != INVALID_IDX) {                                                                   \
            return map_values(*self, K, V) + idx;                                                   \
        }                                                                                           \
        return NULL;                                                                                \
    }                                                                                               \
                                                                                                    \
    map_entry_t Self##_get_entry(Self* self, K key) {                                               \
        usize idx = Self##_get_idx(self, key);                                                      \
        if (idx != INVALID_IDX) {                                                                   \
            return (map_entry_t) {                                                                  \
                .key = map_keys(*self, K) + idx,                                                    \
                .value = map_values(*self, K, V) + idx,                                             \
                .existed = true,                                                                    \
            };                                                                                      \
        }                                                                                           \
        return (map_entry_t) { 0 };                                                                 \
    }                                                                                               \
                                                                                                    \
    bool Self##_contains(Self* self, K key) {                                                       \
        return Self##_get_idx(self, key) != INVALID_IDX;                                            \
    }                                                                                               \
                                                                                                    \
    bool Self##_remove(Self* self, K key) {                                                         \
        usize idx = Self##_get_idx(self, key);                                                      \
        if (idx == INVALID_IDX) return false;                                                       \
                                                                                                    \
        K* keys = map_keys(*self, K);                                                               \
        V* values = map_values(*self, K, V);                                                        \
        memset(keys + idx, 0xDE, sizeof(K));                                                        \
        memset(values + idx, 0xDE, sizeof(V));                                                      \
                                                                                                    \
        self->ptr_[idx] = METADATA_TOMBSTONE;                                                       \
        self->available++;                                                                          \
        self->len--;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_put(Self* self, K key, V value) {                                                   \
        map_entry_t gop = Self##_get_or_put(self, key);                                             \
        if (gop.value == NULL) return false;                                                        \
        *(V*)gop.value = value;                                                                     \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_fetch_put(Self* self, K key, V value, V* out) {                                     \
        map_entry_t gop = Self##_get_or_put(self, key);                                             \
        if (gop.value == NULL) return false;                                                        \
        *out = *(V*)gop.value;                                                                      \
        *(V*)gop.value = value;                                                                     \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    map_entry_t Self##_get_or_put(Self* self, K key) {                                              \
        assert(self != NULL);                                                                       \
                                                                                                    \
        if (self->available == 0) {                                                                 \
            /* TODO: try to get only instead, if allocation fails */                                \
            if (!Self##_ensure_capacity(self, self->capacity + 1)) return (map_entry_t) { 0 };      \
        }                                                                                           \
        K* keys = map_keys(*self, K);                                                               \
        V* values = map_values(*self, K, V);                                                        \
                                                                                                    \
        const u64 hash = hash_fn(&key, sizeof(K));                                                  \
        const u32 mask = self->capacity - 1;                                                        \
        const u8 fingerprint = take_fingerprint(hash);                                              \
                                                                                                    \
        u32 idx = (u32)hash & mask;                                                                 \
        u32 limit = self->capacity;                                                                 \
        u32 first_tombstone_idx = INVALID_IDX;                                                      \
        map_meta_t* meta = self->ptr_ + idx;                                                        \
                                                                                                    \
        while (*meta != METADATA_FREE && limit > 0) {                                               \
            if (is_used(*meta) && get_fingerprint(*meta) == fingerprint) {                          \
                K* test_key = keys + idx;                                                           \
                                                                                                    \
                if (equal_fn(&key, test_key, sizeof(K))) {                                          \
                    return (map_entry_t) {                                                          \
                        .key = test_key,                                                            \
                        .value = values + idx,                                                      \
                        .existed = true,                                                            \
                    };                                                                              \
                }                                                                                   \
            } else if (first_tombstone_idx == INVALID_IDX && *meta == METADATA_TOMBSTONE) {         \
                first_tombstone_idx = idx;                                                          \
            }                                                                                       \
            limit--;                                                                                \
            idx = (idx + 1) & mask;                                                                 \
            meta = self->ptr_ + idx;                                                                \
        }                                                                                           \
                                                                                                    \
        if (first_tombstone_idx != INVALID_IDX) {                                                   \
            idx = first_tombstone_idx;                                                              \
            meta = self->ptr_ + idx;                                                                \
        }                                                                                           \
        *meta = set_fingerprint(fingerprint);                                                       \
        self->available--;                                                                          \
        self->len++;                                                                                \
                                                                                                    \
        keys[idx] = key;                                                                            \
        memset(values + idx, 0, sizeof(V));                                                         \
                                                                                                    \
        return (map_entry_t) {                                                                      \
            .key = keys + idx,                                                                      \
            .value = values + idx,                                                                  \
            .existed = false,                                                                       \
        };                                                                                          \
    }                                                                                               \
                                                                                                    \
    map_iter_t Self##_iter(Self self) {                                                             \
        return (map_iter_t) {                                                                       \
            .meta = self.ptr_,                                                                      \
            .key = map_keys(self, K),                                                               \
            .value = map_values(self, K, V),                                                        \
            .is_first = true,                                                                       \
            .remaining = self.len,                                                                  \
            .capacity = self.capacity,                                                              \
            .key_sizeof = sizeof(K),                                                                \
            .value_sizeof = sizeof(V),                                                              \
        };                                                                                          \
    }
#endif
