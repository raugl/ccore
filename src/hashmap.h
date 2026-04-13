#pragma once
#include "allocator.h"
#include "common.h"

#define HASHMAP_DECL(K, V, name)                                                                   \
    typedef struct {                                                                               \
        void *key, *value;                                                                         \
        bool existed;                                                                              \
    } map_get_or_put_t;                                                                            \
                                                                                                   \
    typedef struct {                                                                               \
        u8* ptr;                                                                                   \
        grow_policy_t policy;                                                                      \
        u32 len, capacity, available;                                                              \
    } map_##name##_t;                                                                              \
                                                                                                   \
    map_##name##_t map_##name##_init_fixed(void* buffer, u32 size);                                \
    map_##name##_t map_##name##_init_arena(arena_t* arena, u32 capacity);                          \
    map_##name##_t map_##name##_init_alloc(allocator_t* alloc, u32 capacity);                      \
    void map_##name##_release(map_##name##_t* self);                                               \
                                                                                                   \
    NULLABLE K* map_##name##_get(map_##name##_t* self, K key);                                     \
    void map_##name##_put(map_##name##_t* self, K key, V value);                                   \
    V map_##name##_fetch_put(map_##name##_t* self, K key, V value);                                \
    map_get_or_put_t map_##name##_get_or_put(map_##name##_t* self, K key);

HASHMAP_DECL(u32, u32, u32_u32)

#ifdef GENERICS_IMPLEMENTATION

// TODO: Try to pull out as much duplicated computation into non-generics helpers

typedef u8 metadata_t;
#define INVALID_IDX               UINT_MAX
#define METADATA_FREE             0x0
#define METADATA_TOMBSTONE        0x80
#define METADATA_USED_MASK        0x80
#define METADATA_FINGERPRINT_MASK 0x7f

#define map_keys(map, K)                                                                           \
    align_forward_ptr((map).ptr + sizeof(metadata_t) * (map).capacity, alignof(K))

#define map_values(map, K, V)                                                                      \
    align_forward_ptr(map_keys((map), K) + sizeof(K) * (map).capacity, alignof(V))

static inline bool is_used(u8 metadata) {
    return (metadata & METADATA_USED_MASK) != 0;
}

static inline u8 set_fingerprint(u8 fingerprint) {
    return fingerprint | METADATA_USED_MASK;
}

static inline u8 get_fingerprint(u8 metadata) {
    return metadata >> 1;
}

#define HASHMAP_IMPL(K, V, name)                                                                   \
    static inline u64 hash_fn(K key) {                                                             \
        return key;                                                                                \
    }                                                                                              \
                                                                                                   \
    static inline bool eql_fn(K a, K b) {                                                          \
        return a == b;                                                                             \
    }                                                                                              \
                                                                                                   \
    map_##name##_t map_##name##_init_fixed(void* buffer, u32 size) {                               \
        assert(buffer != NULL);                                                                    \
                                                                                                   \
        return (map_##name##_t) {                                                                  \
            .ptr = buffer,                                                                         \
            .capacity = size,                                                                      \
            .policy.kind = GROW_POLICY_FIXED,                                                      \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    map_##name##_t map_##name##_init_arena(arena_t* arena, u32 capacity) {                         \
        assert(arena != NULL);                                                                     \
                                                                                                   \
        map_##name##_t self = {                                                                    \
            .policy = {                                                                            \
                .arena = arena,                                                                    \
                .kind = GROW_POLICY_ARENA,                                                         \
            }                                                                                      \
        };                                                                                         \
        return self;                                                                               \
    }                                                                                              \
                                                                                                   \
    map_##name##_t map_##name##_init_alloc(allocator_t* alloc, u32 capacity) {                     \
        assert(alloc != NULL);                                                                     \
    }                                                                                              \
                                                                                                   \
    void map_##name##_release(map_##name##_t* self) {                                              \
        assert(self != NULL);                                                                      \
                                                                                                   \
        switch (self->policy.kind) {                                                               \
        case GROW_POLICY_ARENA:                                                                    \
            panic("attempted to release an arena allocated map");                                  \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            alloc_free(self->policy.alloc, *self);                                                 \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            panic("attempted to release a fixed buffer map");                                      \
            break;                                                                                 \
        }                                                                                          \
        *self = (map_##name##_t) {};                                                               \
    }                                                                                              \
                                                                                                   \
    NULLABLE V* map_##name##_get(map_##name##_t* self, K key) {                                    \
        assert(self != NULL);                                                                      \
                                                                                                   \
        const u64 hash = hash_fn(key);                                                             \
        const u32 mask = self->capacity - 1;                                                       \
        const u8 fingerprint = hash >> 57;                                                         \
                                                                                                   \
        u32 idx = hash & mask;                                                                     \
        u32 limit = self->capacity;                                                                \
        metadata_t* metadata = self->ptr;                                                          \
        K* keys = map_keys(*self, K);                                                              \
                                                                                                   \
        while (*metadata != METADATA_FREE && limit > 0) {                                          \
            if (is_used(*metadata) && get_fingerprint(*metadata) == fingerprint) {                 \
                if (eql_fn(key, keys[idx])) {                                                      \
                    return keys + idx;                                                             \
                }                                                                                  \
            }                                                                                      \
            limit--;                                                                               \
            idx = (idx + 1) & mask;                                                                \
            metadata = self->ptr + idx;                                                            \
        }                                                                                          \
        return NULL;                                                                               \
    }                                                                                              \
                                                                                                   \
    void map_##name##_put(map_##name##_t* self, K key, V value) {                                  \
        assert(self != NULL);                                                                      \
                                                                                                   \
        map_get_or_put_t gop = map_##name##_get_or_put(self, key);                                 \
        *(V*)gop.value = value;                                                                    \
    }                                                                                              \
                                                                                                   \
    V map_##name##_fetch_put(map_##name##_t* self, K key, V value) {                               \
        assert(self != NULL);                                                                      \
                                                                                                   \
        map_get_or_put_t gop = map_##name##_get_or_put(self, key);                                 \
        V result = *(V*)gop.value;                                                                 \
        *(V*)gop.value = value;                                                                    \
        return result;                                                                             \
    }                                                                                              \
                                                                                                   \
    map_get_or_put_t map_##name##_get_or_put(map_##name##_t* self, K key) {                        \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->available == 0) {}                                                               \
                                                                                                   \
        const u64 hash = hash_fn(key);                                                             \
        const u32 mask = self->capacity - 1;                                                       \
                                                                                                   \
        K* keys = map_keys(*self, K);                                                              \
        V* values = map_values(*self, K, V);                                                       \
        const u8 fingerprint = hash >> 57;                                                         \
                                                                                                   \
        u32 idx = hash & mask;                                                                     \
        u32 limit = self->capacity;                                                                \
        u32 first_tombstone_idx = INVALID_IDX;                                                     \
        metadata_t* metadata = self->ptr;                                                          \
                                                                                                   \
        while (*metadata != METADATA_FREE && limit > 0) {                                          \
            if (is_used(*metadata) && get_fingerprint(*metadata) == fingerprint) {                 \
                K* test_key = keys + idx;                                                          \
                                                                                                   \
                if (eql_fn(key, *test_key)) {                                                      \
                    return (map_get_or_put_t) {                                                    \
                        .key = test_key,                                                           \
                        .value = values + idx,                                                     \
                        .existed = true,                                                           \
                    };                                                                             \
                }                                                                                  \
            } else if (first_tombstone_idx == INVALID_IDX && *metadata == METADATA_TOMBSTONE) {    \
                first_tombstone_idx = idx;                                                         \
            }                                                                                      \
            limit--;                                                                               \
            idx = (idx + 1) & mask;                                                                \
            metadata = self->ptr + idx;                                                            \
        }                                                                                          \
                                                                                                   \
        if (first_tombstone_idx != INVALID_IDX) {                                                  \
            idx = first_tombstone_idx;                                                             \
            metadata = self->ptr + idx;                                                            \
        }                                                                                          \
        *metadata = set_fingerprint(fingerprint);                                                  \
        self->available--;                                                                         \
        self->len++;                                                                               \
                                                                                                   \
        ((K*)keys)[idx] = key;                                                                     \
        ((V*)values)[idx] = (V) {};                                                                \
                                                                                                   \
        return (map_get_or_put_t) {                                                                \
            .key = keys + idx,                                                                     \
            .value = values + idx,                                                                 \
            .existed = false,                                                                      \
        };                                                                                         \
    }

// =================================================================================================
// TODO: Figure out a way to move these comments into the HASHMAP_IMPL macro's body
// =================================================================================================
// static inline u64 hash_fn(u32 key) {
//     return key; // FIXME: Implement Wyhash
// }
//
// static inline bool eql_fn(u32 a, u32 b) {
//     return a == b;
// }
//
// // NOTE: The alignment doesn't matter as the first section of the buffers is used by metadata bytes.
// // Past that I manually add padding to align the keys and values.
// map_u32_u32_t map_u32_u32_init_fixed(void* buffer, u32 size) {
//     assert(buffer != NULL);
//
//     return (map_u32_u32_t) {
//         .ptr = buffer,
//         // FIXME: comptute size /= sizeof(metadata_t) + sizeof(K) + sizeof(V) + random padding
//         .capacity = size,
//         .policy.kind = GROW_POLICY_FIXED,
//     };
// }
//
// map_u32_u32_t map_u32_u32_init_arena(arena_t* arena, u32 capacity) {
//     assert(arena != NULL);
//
//     map_u32_u32_t self = {
//         .policy = {
//             .arena = arena,
//             .kind = GROW_POLICY_ARENA,
//         }
//     };
//     // FIXME: comptute size /= sizeof(metadata_t) + sizeof(K) + sizeof(V) + random padding
//     // map_u32_u32_grow(&self, capacity);
//     return self;
// }
//
// map_u32_u32_t map_u32_u32_init_alloc(allocator_t* alloc, u32 capacity) {
//     assert(alloc != NULL);
//     // TODO:
// }
//
// NULLABLE u32* map_u32_u32_get(map_u32_u32_t* self, u32 key) {
//     assert(self != NULL);
//
//     const u64 hash = hash_fn(key);
//     const u32 mask = self->capacity - 1;
//     const u8 fingerprint = hash >> 57;
//
//     u32 idx = hash & mask;
//     u32 limit = self->capacity;
//     metadata_t* metadata = self->ptr;
//     u32* keys = map_keys(*self, u32);
//
//     // PERF: Could make use of simd to probe faster for `fingerprint`
//     while (*metadata != METADATA_FREE && limit > 0) {
//         if (is_used(*metadata) && get_fingerprint(*metadata) == fingerprint) {
//             if (eql_fn(key, keys[idx])) {
//                 return keys + idx;
//             }
//         }
//         limit--;
//         idx = (idx + 1) & mask;
//         metadata = self->ptr + idx;
//     }
//     return NULL;
// }
//
// map_get_or_put_t map_u32_u32_get_or_put(map_u32_u32_t* self, u32 key) {
//     assert(self != NULL);
//
//     if (self->available == 0) {
//         // TODO: grow
//     }
//
//     const u64 hash = hash_fn(key);
//     const u32 mask = self->capacity - 1;
//
//     u32* keys = map_keys(*self, u32);
//     u32* values = map_values(*self, u32, u32);
//     const u8 fingerprint = hash >> 57;
//
//     u32 idx = hash & mask;
//     u32 limit = self->capacity;
//     u32 first_tombstone_idx = INVALID_IDX;
//     metadata_t* metadata = self->ptr;
//
//     // PERF: Could make use of simd to probe faster for `fingerprint`
//     while (*metadata != METADATA_FREE && limit > 0) {
//         if (is_used(*metadata) && get_fingerprint(*metadata) == fingerprint) {
//             u32* test_key = keys + idx;
//
//             if (eql_fn(key, *test_key)) {
//                 return (map_get_or_put_t) {
//                     .key = test_key,
//                     .value = values + idx,
//                     .existed = true,
//                 };
//             }
//         } else if (first_tombstone_idx == INVALID_IDX && *metadata == METADATA_TOMBSTONE) {
//             first_tombstone_idx = idx;
//         }
//         limit--;
//         idx = (idx + 1) & mask;
//         metadata = self->ptr + idx;
//     }
//
//     if (first_tombstone_idx != INVALID_IDX) {
//         idx = first_tombstone_idx;
//         metadata = self->ptr + idx;
//     }
//     *metadata = set_fingerprint(fingerprint);
//     self->available--;
//     self->len++;
//
//     ((u32*)keys)[idx] = key;
//     ((u32*)values)[idx] = (u32) {};
//
//     return (map_get_or_put_t) {
//         .key = keys + idx,
//         .value = values + idx,
//         .existed = false,
//     };
// }
#endif
