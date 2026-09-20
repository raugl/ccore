#pragma once
#include "common.h"
#include "slice.h"
#include "allocator.h"

// TODO: Add an option to declare all the map's functions as static, could even use the same macro.
// Would be useful for creating one-off local instantiations without namespacing them to hell.

// TODO: Probably add a clone function at some point.
typedef u8 map_metadata;

typedef struct raw_hashmap {
    u8*         _data;
    allocator_t _allocator;
    u32         len, capacity, _available;
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
        if ((*self->_meta & 0x80) != 0) { // NOTE: if (metadata_used(*self->_meta))
            self->_remaining--;
            return true;
        }
    }
    panic("iterator ran off the hashmap's end while probing for %d remaining entries", self->_remaining);
}

// FIXME: I think that I can actually use rehash to grow the hashmap in place. I should use
// realloc, but I'm not sure that in-place rehash isnt meaningfully slower than just inserting in a
// new buffer.
#define hashmap_buffer_size(K, V, capacity) \
    ((sizeof(map_metadata) + sizeof(K) + sizeof(V)) * capacity + sizeof(max_align_t) * 2)

#define HASHMAP_DECL(K, V) HASHMAP_DECL_RAW(K, V, map_##K##_##V)

#define HASHMAP_DECL_RAW(K, V, Self)                                                                \
    typedef struct Self {                                                                           \
        u8*         _data;                                                                          \
        allocator_t _allocator;                                                                     \
        u32         len, capacity, _available;                                                      \
    } Self;                                                                                         \
                                                                                                    \
    /* The buffer's size should be obtained with `hashmap_buffer_size`.
     * The capacity should represent elements, not the size in bytes of the buffer. */              \
    Self Self##_init_fixed(void* buffer, usize capacity);                                           \
    Self Self##_init_alloc(allocator_t allocator);                                                  \
    void Self##_destroy(Self* self);                                                                \
                                                                                                    \
    /* Ensures that the hashmap can hold at least `capacity` items.
     * Rounds up the capacity to the nearest power of 2. */                                         \
    bool Self##_reserve(Self* self, usize capacity);                                                \
                                                                                                    \
    /* Ensures that the hashmap can hold at least `count` extra items, on top of the already
     * resident ones. Rounds up the total capacity to the nearest power of 2. */                    \
    bool Self##_reserve_spare(Self* self, usize count);                                             \
                                                                                                    \
    /* Shrinks the hashmap to use less memory. The new capacity will be `current_len / load_factor`.
     * This triggers a rehash. `can_realloc` determines if the current buffer, after a failure to
     * shrink in place, should be reallocated. Note that for heap allocators this is desirable, but
     * for an arena allocator this would actually result in an even heigher memory footprint. */    \
    bool Self##_shrink_to_fit(Self* self, bool can_realloc);                                        \
                                                                                                    \
    /* Empties the hashmap, the old memory buffer is retained. */                                   \
    void Self##_clear(Self* self);                                                                  \
                                                                                                    \
    /* Rehash the map, in-place. Due to the tombstone implementation, the hashmap can become slow
     * after many `insert`s and `remove`s. After this function is called, there will be no tombstones
     * in the hashmap, each of the entries is rehashed and any existing pointers or iterators into
     * the hashmap are invalidated. */                                                              \
    void Self##_rehash(Self* self);                                                                 \
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

HASHMAP_DECL_RAW(u32, u32, map_u32)
HASHMAP_DECL_RAW(string, u64, map_str_u64)

#ifdef GENERICS_IMPLEMENTATION
#include "hash.h"
#include "math.h"
#include "slice.h"
#include "testing.h"

#define INVALID_IDX               UINT_MAX
#define MIN_CAPACITY              8
#define METADATA_FREE             0x0
#define METADATA_USED             0x80
#define METADATA_TOMBSTONE        0x01
#define METADATA_REHASHED         (METADATA_USED | METADATA_TOMBSTONE)
#define METADATA_USED_MASK        0x80
#define METADATA_FINGERPRINT_MASK 0x7F

// FIXME: NOTHING in here takes into account `_available`. And all the reserve functions try to fill
// all the way to the top. They should all respect the load factor.

#define map_keys(map, K)                                                                            \
    ((K*)align_forward_ptr((map)._data + sizeof(map_metadata) * (map).capacity, alignof(K)))

#define map_values(map, K, V)                                                                       \
    ((V*)align_forward_ptr(map_keys((map), K) + (map).capacity, alignof(V)))

FORCE_INLINE bool metadata_free(u8 meta) {
    return meta == METADATA_FREE;
}

FORCE_INLINE bool metadata_used(u8 meta) {
    return (meta & METADATA_USED_MASK) != 0;
}

FORCE_INLINE u8 set_fingerprint(u8 fingerprint) {
    assert_u8(fingerprint, <, METADATA_USED_MASK);
    return fingerprint | METADATA_USED_MASK;
}

FORCE_INLINE u8 get_fingerprint(u8 meta) {
    return meta & METADATA_FINGERPRINT_MASK;
}

FORCE_INLINE u8 take_fingerprint(u64 hash) {
    return (u8)(hash >> 59);
}

static u64 hash_slice(const void* key, usize size) {
    const slice_raw* slice = key;
    (void)size;

    return hash_bytes(slice->data, slice->len);
}

static bool equal_slice(const void* a, const void* b, usize size) {
    const slice_raw *slice_a = a, *slice_b = b;
    (void)size;

    if (slice_a->len != slice_b->len) return false;
    if (slice_a->len == 0 || slice_a->data == slice_b->data) return true;
    return memcmp(slice_a->data, slice_b->data, slice_a->len) == 0;
}

FORCE_INLINE bool equal_bytes(const void* a, const void* b, usize size) {
    if (a == b) return true;
    return memcmp(a, b, size) == 0;
}

FORCE_INLINE bool equal_ptr(const rawptr* a, const rawptr* b, usize size) {
    assert(size == sizeof(void*));
    return (*a == *b);
}

#define HASHMAP_IMPL(K, V, hash_proc, equal_proc)                                                   \
    HASHMAP_IMPL_RAW(K, V, map_##K##_##V, hash_proc, equal_proc, 75)

#define HASHMAP_IMPL_RAW(K, V, Self, hash_proc, equal_proc, LOAD_FACTOR)                            \
    Self Self##_init_fixed(void* buffer, usize capacity) {                                          \
        assert(buffer != NULL);                                                                     \
        assert(is_pow2(capacity));                                                                  \
        memset(buffer, METADATA_FREE, capacity * sizeof(map_metadata));                             \
                                                                                                    \
        return (Self) {                                                                             \
            ._data      = buffer,                                                                   \
            .capacity   = (u32)capacity,                                                            \
            ._available = (u32)(capacity * LOAD_FACTOR / 100),                                      \
            ._allocator = allocator_init_null(),                                                    \
        };                                                                                          \
    }                                                                                               \
                                                                                                    \
    Self Self##_init_alloc(allocator_t allocator) {                                                 \
        assert(allocator.proc != NULL);                                                             \
        return (Self) { ._allocator = allocator };                                                  \
    }                                                                                               \
                                                                                                    \
    void Self##_destroy(Self* self) {                                                               \
        assert(self != NULL);                                                                       \
                                                                                                    \
        usize size = 0;                                                                             \
        size += self->capacity * sizeof(map_metadata);                                              \
        size = align_forward(size, alignof(K));                                                     \
        size += self->capacity * sizeof(K);                                                         \
        size = align_forward(size, alignof(V));                                                     \
        size += self->capacity * sizeof(V);                                                         \
                                                                                                    \
        /* mem_free(self->_allocator, (u8*)self->ptr_, size); */ /* TODO: Test catch me */          \
        memset_destroyed(self, sizeof(Self));                                                       \
    }                                                                                               \
                                                                                                    \
    static usize Self##_get_index(Self self, K key) {                                               \
        K* keys = map_keys(self, K);                                                                \
                                                                                                    \
        const u64 hash = hash_proc(&key, sizeof(K));                                                \
        const u32 mask = self.capacity - 1;                                                         \
        const u8 fingerprint = take_fingerprint(hash);                                              \
                                                                                                    \
        u32 index = (u32)hash & mask;                                                               \
        u32 limit = self.capacity;                                                                  \
        map_metadata* meta = &self._data[index];                                                    \
                                                                                                    \
        while (!metadata_free(*meta) && limit > 0) {                                                \
            if (metadata_used(*meta) && get_fingerprint(*meta) == fingerprint) {                    \
                if (equal_proc(&key, &keys[index], sizeof(K))) {                                    \
                    return index;                                                                   \
                }                                                                                   \
            }                                                                                       \
            limit--;                                                                                \
            index = (index + 1) & mask;                                                             \
            meta = &self._data[index];                                                              \
        }                                                                                           \
        return INVALID_IDX;                                                                         \
    }                                                                                               \
                                                                                                    \
    static void Self##_insert_assume_capacity_no_clobber(Self* self, K key, V value) {              \
        const u64 hash = hash_proc(&key, sizeof(K));                                                \
        const u32 mask = self->capacity - 1;                                                        \
        const u8 fingerprint = take_fingerprint(hash);                                              \
                                                                                                    \
        u32 index = (u32)hash & mask;                                                               \
        map_metadata* meta = &self->_data[index];                                                   \
                                                                                                    \
        while (metadata_used(*meta)) {                                                              \
            index = (index + 1) & mask;                                                             \
            meta = &self->_data[index];                                                             \
        }                                                                                           \
                                                                                                    \
        K* keys = map_keys(*self, K);                                                               \
        V* values = map_values(*self, K, V);                                                        \
        assert(self->_available > 0);                                                               \
                                                                                                    \
        *meta = set_fingerprint(fingerprint);                                                       \
        self->_available--;                                                                         \
        self->len++;                                                                                \
                                                                                                    \
        keys[index] = key;                                                                          \
        values[index] = value;                                                                      \
    }                                                                                               \
                                                                                                    \
    static void Self##_rehash_smaller_buffer(Self* self, usize new_capacity) {                      \
        assert(self != NULL);                                                                       \
        assert(new_capacity < self->capacity);                                                      \
                                                                                                    \
        K* keys = map_keys(*self, K);                                                               \
        V* values = map_values(*self, K, V);                                                        \
        map_metadata* metas = self->_data;                                                          \
                                                                                                    \
        /* While rehashing every slot, use the metadata to mean either free, used, or rehashed.
         * Rehashed means that we will revist it and pick a new bucket at least one more time. */   \
        for (u32 i = 0; i < self->capacity; ++i) {                                                  \
            metas[i] &= METADATA_USED_MASK;                                                         \
        }                                                                                           \
                                                                                                    \
        for (u32 curr = 0; curr < self->capacity; ++curr) {                                         \
            if (!metadata_used(metas[curr])) {                                                      \
                assert(metadata_free(metas[curr]));                                                 \
                continue;                                                                           \
            }                                                                                       \
            const u32 mask = (u32)new_capacity - 1;                                                 \
            const u64 hash = hash_proc(&keys[curr], sizeof(K));                                     \
            const u8 fingerprint = take_fingerprint(hash);                                          \
            u32 index = (u32)hash & mask;                                                           \
                                                                                                    \
            while ((index < curr && metadata_used(metas[index])) ||                                 \
                (index > curr && metas[index] == METADATA_REHASHED)) index = (index + 1) & mask;    \
                                                                                                    \
            if (index < curr) {                                                                     \
                assert(metadata_free(metas[index]));                                                \
                metas[index] = set_fingerprint(fingerprint);                                        \
                keys[index] = keys[curr];                                                           \
                values[index] = values[curr];                                                       \
                                                                                                    \
                metas[curr] = METADATA_FREE;                                                        \
                memset_destroyed(&keys[curr], sizeof(K));                                           \
                memset_destroyed(&values[curr], sizeof(V));                                         \
            } else if (index == curr) {                                                             \
                metas[index] = set_fingerprint(fingerprint);                                        \
            } else {                                                                                \
                assert_u8(metas[curr], ==, METADATA_REHASHED);                                      \
                metas[index] = METADATA_REHASHED;                                                   \
                                                                                                    \
                if (metadata_used(metas[index])) {                                                  \
                    K tmp_key = keys[curr];                                                         \
                    keys[curr] = keys[index];                                                       \
                    keys[index] = tmp_key;                                                          \
                                                                                                    \
                    V tmp_value = values[curr];                                                     \
                    values[curr] = values[index];                                                   \
                    values[index] = tmp_value;                                                      \
                    curr--;                                                                         \
                } else {                                                                            \
                    keys[index] = keys[curr];                                                       \
                    values[index] = values[curr];                                                   \
                                                                                                    \
                    metas[curr] = METADATA_FREE;                                                    \
                    memset_destroyed(&keys[curr], sizeof(K));                                       \
                    memset_destroyed(&values[curr], sizeof(V));                                     \
                }                                                                                   \
            }                                                                                       \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve(Self* self, usize new_capacity) {                                           \
        assert(self != NULL);                                                                       \
        new_capacity = next_pow2_u64(max_usize(MIN_CAPACITY, new_capacity));                        \
        if (self->capacity > new_capacity) return true;                                             \
                                                                                                    \
        usize size = 0;                                                                             \
        size += new_capacity * sizeof(map_metadata);                                                \
        size = align_forward(size, alignof(K));                                                     \
        size += new_capacity * sizeof(K);                                                           \
        size = align_forward(size, alignof(V));                                                     \
        size += new_capacity * sizeof(V);                                                           \
        usize align = max_usize(alignof(K), alignof(V));                                            \
                                                                                                    \
        u8* new_ptr = mem_alloc_aligned(self->_allocator, u8, size, align);                         \
        if (new_ptr == NULL) return false;                                                          \
        memset(new_ptr, METADATA_FREE, new_capacity * sizeof(map_metadata));                        \
                                                                                                    \
        Self new_map = {                                                                            \
            ._data      = new_ptr,                                                                  \
            .capacity   = (u32)new_capacity,                                                        \
            ._available = (u32)(new_capacity * LOAD_FACTOR / 100),                                  \
            ._allocator = self->_allocator,                                                         \
        };                                                                                          \
        if (self->len > 0) {                                                                        \
            K* keys = map_keys(*self, K);                                                           \
            V* values = map_values(*self, K, V);                                                    \
            map_metadata* metas = self->_data;                                                      \
                                                                                                    \
            for (usize i = 0; i < self->capacity; ++i) {                                            \
                if (!metadata_used(metas[i])) continue;                                             \
                Self##_insert_assume_capacity_no_clobber(&new_map, keys[i], values[i]);             \
                if (self->len == new_map.len) break;                                                \
            }                                                                                       \
        }                                                                                           \
        Self##_destroy(self);                                                                       \
        *self = new_map;                                                                            \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve_spare(Self* self, usize count) {                                            \
        assert(self != NULL);                                                                       \
        return Self##_reserve(self, self->len + count);                                             \
    }                                                                                               \
                                                                                                    \
    bool Self##_shrink_to_fit(Self* self, bool can_realloc) {                                       \
        assert(self != NULL);                                                                       \
        usize new_capacity = next_pow2_u64(max_usize(MIN_CAPACITY, self->len));                     \
                                                                                                    \
        usize old_size = 0;                                                                         \
        old_size += self->capacity * sizeof(map_metadata);                                          \
        old_size = align_forward(old_size, alignof(K));                                             \
        old_size += self->capacity * sizeof(K);                                                     \
        old_size = align_forward(old_size, alignof(V));                                             \
        old_size += self->capacity * sizeof(V);                                                     \
                                                                                                    \
        usize new_size = 0;                                                                         \
        new_size += new_capacity * sizeof(map_metadata);                                            \
        new_size = align_forward(new_size, alignof(K));                                             \
        new_size += new_capacity * sizeof(K);                                                       \
        new_size = align_forward(new_size, alignof(V));                                             \
        new_size += new_capacity * sizeof(V);                                                       \
                                                                                                    \
        if (mem_resize(self->_allocator, self->_data, old_size, new_size) && can_realloc) {         \
            Self##_rehash_smaller_buffer(self, new_capacity);                                       \
            return false;                                                                           \
        } else {                                                                                    \
            if (!can_realloc) return false;                                                         \
            u8* new_ptr = mem_realloc(self->_allocator, self->_data, old_size, new_size);           \
                                                                                                    \
            if (new_ptr == NULL) return false;                                                      \
            Self##_rehash_smaller_buffer(self, new_capacity);                                       \
            return false;                                                                           \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    void Self##_clear(Self* self) {                                                                 \
        assert(self != NULL);                                                                       \
        self->len = 0;                                                                              \
        self->_available = (self->capacity * LOAD_FACTOR / 100),                                    \
                                                                                                    \
        memset_destroyed(map_keys(*self, K), self->capacity * sizeof(K));                           \
        memset_destroyed(map_values(*self, K, V), self->capacity * sizeof(V));                      \
        memset(self->_data, METADATA_FREE, self->capacity * sizeof(map_metadata));                  \
    }                                                                                               \
                                                                                                    \
    void Self##_rehash(Self* self) {                                                                \
        assert(self != NULL);                                                                       \
        Self##_rehash_smaller_buffer(self, self->capacity);                                         \
    }                                                                                               \
                                                                                                    \
    bool Self##_contains(Self self, K key) {                                                        \
        return Self##_get_index(self, key) != INVALID_IDX;                                          \
    }                                                                                               \
                                                                                                    \
    bool Self##_get(Self self, K key, V** value) {                                                  \
        usize index = Self##_get_index(self, key);                                                  \
        if (index == INVALID_IDX) return false;                                                     \
        *value = &map_values(self, K, V)[index];                                                    \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_insert(Self* self, K key, V value) {                                                \
        assert(self != NULL);                                                                       \
        V* value_ptr;                                                                               \
        if (!Self##_fetch_insert(self, key, &value_ptr, NULL)) return false;                        \
        *value_ptr = value;                                                                         \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_fetch_insert(Self* self, K key, V** value, bool* existed) {                         \
        assert(self != NULL);                                                                       \
        K* keys = map_keys(*self, K);                                                               \
        V* values = map_values(*self, K, V);                                                        \
                                                                                                    \
        if (!Self##_reserve_spare(self, 1)) {                                                       \
            /* If allocation fails, try to do the lookup anyway. If we find an existing item,
             * we can return it. Otherwise return the error, we could not add another. */           \
            usize index = Self##_get_index(*self, key);                                             \
            if (index == INVALID_IDX) return false;                                                 \
                                                                                                    \
            *value = &values[index];                                                                \
            if (existed != NULL) *existed = true;                                                   \
            return true;                                                                            \
        }                                                                                           \
        const u64 hash = hash_proc(&key, sizeof(K));                                                \
        const u32 mask = self->capacity - 1;                                                        \
        const u8 fingerprint = take_fingerprint(hash);                                              \
                                                                                                    \
        u32 index = (u32)hash & mask;                                                               \
        u32 limit = self->capacity;                                                                 \
        u32 first_tombstone_idx = INVALID_IDX;                                                      \
        map_metadata* meta = &self->_data[index];                                                   \
                                                                                                    \
        while (!metadata_free(*meta) && limit > 0) {                                                \
            if (metadata_used(*meta) && get_fingerprint(*meta) == fingerprint) {                    \
                K* test_key = &keys[index];                                                         \
                                                                                                    \
                if (equal_proc(&key, test_key, sizeof(K))) {                                        \
                    *value = &values[index];                                                        \
                    if (existed != NULL) *existed = true;                                           \
                    return true;                                                                    \
                }                                                                                   \
            } else if (first_tombstone_idx == INVALID_IDX && *meta == METADATA_TOMBSTONE) {         \
                first_tombstone_idx = index;                                                        \
            }                                                                                       \
            limit--;                                                                                \
            index = (index + 1) & mask;                                                             \
            meta = &self->_data[index];                                                             \
        }                                                                                           \
                                                                                                    \
        if (first_tombstone_idx != INVALID_IDX) {                                                   \
            index = first_tombstone_idx;                                                            \
            meta = &self->_data[index];                                                             \
        }                                                                                           \
        *meta = set_fingerprint(fingerprint);                                                       \
        self->_available--;                                                                         \
        self->len++;                                                                                \
                                                                                                    \
        keys[index] = key;                                                                          \
        memset_undefined(values + index, sizeof(V));                                                \
        *value = &values[index];                                                                    \
        if (existed != NULL) *existed = false;                                                      \
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
        K* keys = map_keys(*self, K);                                                               \
        V* values = map_values(*self, K, V);                                                        \
                                                                                                    \
        usize index = Self##_get_index(*self, key);                                                 \
        if (index == INVALID_IDX) return false;                                                     \
                                                                                                    \
        *value = values[index];                                                                     \
        memset_destroyed(&keys[index], sizeof(K));                                                  \
        memset_destroyed(&values[index], sizeof(V));                                                \
                                                                                                    \
        self->_data[index] = METADATA_TOMBSTONE;                                                    \
        self->_available++;                                                                         \
        self->len--;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    map_iterator Self##_iterator(Self self) {                                                       \
        return (map_iterator) {                                                                     \
            ._meta         = self._data,                                                            \
            .key           = map_keys(self, K),                                                     \
            .value         = map_values(self, K, V),                                                \
            ._is_first     = true,                                                                  \
            ._remaining    = self.len,                                                              \
            ._capacity     = self.capacity,                                                         \
            ._key_sizeof   = sizeof(K),                                                             \
            ._value_sizeof = sizeof(V),                                                             \
        };                                                                                          \
    }
#endif
