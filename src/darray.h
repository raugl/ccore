#pragma once
#include "allocator.h"
#include "common.h"

#define DARRAY_DECL_RAW(T, name, P)                                                                \
    typedef struct {                                                                               \
        T* ptr;                                                                                    \
        grow_policy_t grow_;                                                                       \
        u32 len, capacity;                                                                         \
    } name##_t;                                                                                    \
                                                                                                   \
    name##_t name##_init_fixed(T* buffer, usize capacity);                                         \
    name##_t name##_init_arena(arena_t* arena);                                                    \
    name##_t name##_init_alloc(allocator_t* alloc);                                                \
    void name##_release(name##_t* self);                                                           \
                                                                                                   \
    bool name##_ensure_capacity(name##_t* self, usize new_capacity);                               \
    bool name##_resize(name##_t* self, usize new_len);                                             \
    void name##_shrink_to_fit(name##_t* self);                                                     \
    void name##_clear(name##_t* self);                                                             \
                                                                                                   \
    bool name##_push(name##_t* self, T item);                                                      \
    bool name##_pop(name##_t* self, T* out);                                                       \
    bool name##_back(name##_t* self, T* out);                                                      \
    bool name##_insert(name##_t* self, usize idx, T item);                                         \
    T name##_remove(name##_t* self, usize idx);                                                    \
    T name##_swap_remove(name##_t* self, usize idx);                                               \
    bool name##_push_many(name##_t* self, P items, usize len);                                     \
    bool name##_insert_many(name##_t* self, usize idx, P items, usize len);                        \
    void name##_remove_many(name##_t* self, usize idx, usize len);                                 \
    void name##_swap_remove_many(name##_t* self, usize idx, usize len);

#define DARRAY_DECL(T) DARRAY_DECL_RAW(T, darray_##T, const T*)

DARRAY_DECL_RAW(u8, darray_u8, const void*)
DARRAY_DECL(u32)
DARRAY_DECL(u64)
DARRAY_DECL(f32)

#ifdef GENERICS_IMPLEMENTATION
#include "math.h"

#define MIN_CAPACITY 8

#define DARRAY_IMPL_RAW(T, name, P)                                                                \
    name##_t name##_init_fixed(T* buffer, usize capacity) {                                        \
        assert(buffer != NULL);                                                                    \
        return (name##_t) {                                                                        \
            .ptr = buffer,                                                                         \
            .capacity = (u32)capacity,                                                             \
            .grow_.kind = GROW_POLICY_FIXED,                                                       \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_arena(arena_t* arena) {                                                   \
        assert(arena != NULL);                                                                     \
        return (name##_t) {                                                                        \
            .grow_.arena = arena,                                                                  \
            .grow_.kind = GROW_POLICY_ARENA,                                                       \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_alloc(allocator_t* alloc) {                                               \
        assert(alloc != NULL);                                                                     \
        return (name##_t) {                                                                        \
            .grow_.alloc = alloc,                                                                  \
            .grow_.kind = GROW_POLICY_ALLOC,                                                       \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    void name##_release(name##_t* self) {                                                          \
        assert(self != NULL);                                                                      \
                                                                                                   \
        switch (self->grow_.kind) {                                                                \
        case GROW_POLICY_ARENA:                                                                    \
            /* TODO: Discard the result, this is a best effort and always safe. */                 \
            arena_try_resize(self->grow_.arena, self->ptr, self->capacity * sizeof(T), 0);         \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            alloc_free(self->grow_.alloc, self->ptr, self->capacity * sizeof(T));                  \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            memset(self->ptr, 0xDE, self->capacity * sizeof(T));                                   \
            break;                                                                                 \
        }                                                                                          \
        memset(self->ptr, 0xDE, self->len * sizeof(T));                                            \
        memset(self, 0xDE, sizeof(name##_t));                                                      \
    }                                                                                              \
                                                                                                   \
    /* FIXME: somehow try to detect u32 overflow for the new_capacity and return out of memory */  \
    bool name##_ensure_capacity(name##_t* self, usize new_capacity) {                              \
        assert(self != NULL);                                                                      \
        if (self->capacity >= new_capacity) return true;                                           \
        new_capacity = next_pow2_u64(max_usize(MIN_CAPACITY, new_capacity));                       \
        T* new_ptr;                                                                                \
                                                                                                   \
        switch (self->grow_.kind) {                                                                \
        case GROW_POLICY_ARENA:                                                                    \
            new_ptr = arena_realloc(                                                               \
                self->grow_.arena,                                                                 \
                self->ptr,                                                                         \
                self->capacity * sizeof(T),                                                        \
                new_capacity * sizeof(T)                                                           \
            );                                                                                     \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            new_ptr = alloc_realloc(                                                               \
                self->grow_.alloc,                                                                 \
                self->ptr,                                                                         \
                self->capacity * sizeof(T),                                                        \
                new_capacity * sizeof(T)                                                           \
            );                                                                                     \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            return false;                                                                          \
        }                                                                                          \
        if (new_ptr == NULL) return false;                                                         \
        self->capacity = (u32)new_capacity;                                                        \
        self->ptr = new_ptr;                                                                       \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_resize(name##_t* self, usize new_len) {                                            \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->capacity < new_len) {                                                            \
            if (!name##_ensure_capacity(self, new_len)) return false;                              \
        } else if (self->len > new_len) {                                                          \
            memset(self->ptr + new_len, 0xDE, (self->len - new_len) * sizeof(T));                  \
        }                                                                                          \
        self->len = (u32)new_len;                                                                  \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    void name##_shrink_to_fit(name##_t* self) {                                                    \
        assert(self != NULL);                                                                      \
                                                                                                   \
        switch (self->grow_.kind) {                                                                \
        case GROW_POLICY_ARENA:                                                                    \
            log_warn("attempted to shrink an arena allocated darray");                             \
            break;                                                                                 \
        case GROW_POLICY_ALLOC: {                                                                  \
            usize new_capacity = self->len;                                                        \
            if (new_capacity <= self->capacity / 4) new_capacity = next_pow2_u64(new_capacity);    \
            if (new_capacity >= self->capacity) break;                                             \
                                                                                                   \
            T* new_ptr = alloc_realloc(                                                            \
                self->grow_.alloc,                                                                 \
                self->ptr,                                                                         \
                self->capacity * sizeof(T),                                                        \
                new_capacity * sizeof(T)                                                           \
            );                                                                                     \
            if (new_ptr != NULL) {                                                                 \
                self->capacity = (u32)new_capacity;                                                \
                self->ptr = new_ptr;                                                               \
            }                                                                                      \
            break;                                                                                 \
        }                                                                                          \
        case GROW_POLICY_FIXED:                                                                    \
            log_warn("attempted to shrink a fixed buffer darray");                                 \
            break;                                                                                 \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    void name##_clear(name##_t* self) {                                                            \
        assert(self != NULL);                                                                      \
                                                                                                   \
        memset(self->ptr, 0xDE, self->len * sizeof(T));                                            \
        self->len = 0;                                                                             \
    }                                                                                              \
                                                                                                   \
    bool name##_insert(name##_t* self, usize idx, T item) {                                        \
        return name##_insert_many(self, idx, &item, 1);                                            \
    }                                                                                              \
                                                                                                   \
    bool name##_insert_many(name##_t* self, usize idx, P items, usize len) {                       \
        assert(self != NULL);                                                                      \
        assert(items != NULL);                                                                     \
        assert(idx <= self->len);                                                                  \
                                                                                                   \
        if (self->len + len >= self->capacity) {                                                   \
            if (!name##_ensure_capacity(self, self->len + len)) return false;                      \
        }                                                                                          \
        memmove(self->ptr + idx + len, self->ptr + idx, (self->len - idx) * sizeof(T));            \
        memcpy(self->ptr + idx, items, len * sizeof(T));                                           \
        self->len += len;                                                                          \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    T name##_remove(name##_t* self, usize idx) {                                                   \
        assert(self != NULL);                                                                      \
        assert(idx < self->len);                                                                   \
                                                                                                   \
        T old_val = self->ptr[idx];                                                                \
        name##_remove_many(self, idx, 1);                                                          \
        return old_val;                                                                            \
    }                                                                                              \
                                                                                                   \
    void name##_remove_many(name##_t* self, usize idx, usize len) {                                \
        assert(self != NULL);                                                                      \
        assert(idx + len <= self->len);                                                            \
                                                                                                   \
        memmove(self->ptr + idx, self->ptr + idx + len, self->len - idx * sizeof(T));              \
        memset(self->ptr + (self->len - len), 0xDE, len * sizeof(T));                              \
        self->len -= len;                                                                          \
    }                                                                                              \
                                                                                                   \
    T name##_swap_remove(name##_t* self, usize idx) {                                              \
        assert(self != NULL);                                                                      \
        assert(idx < self->len);                                                                   \
                                                                                                   \
        T old_val = self->ptr[idx];                                                                \
        name##_swap_remove_many(self, idx, 1);                                                     \
        return old_val;                                                                            \
    }                                                                                              \
                                                                                                   \
    void name##_swap_remove_many(name##_t* self, usize idx, usize len) {                           \
        assert(self != NULL);                                                                      \
        assert(idx + len <= self->len);                                                            \
                                                                                                   \
        memmove(self->ptr + idx, self->ptr + (self->len - len), len * sizeof(T));                  \
        memset(self->ptr + (self->len - len), 0xDE, len * sizeof(T));                              \
        self->len -= len;                                                                          \
    }                                                                                              \
                                                                                                   \
    bool name##_push(name##_t* self, T item) {                                                     \
        return name##_push_many(self, &item, 1);                                                   \
    }                                                                                              \
                                                                                                   \
    bool name##_push_many(name##_t* self, P items, usize len) {                                    \
        assert(self != NULL);                                                                      \
        assert(items != NULL);                                                                     \
                                                                                                   \
        if (self->len + len >= self->capacity) {                                                   \
            if (!name##_ensure_capacity(self, self->len + len)) return false;                      \
        }                                                                                          \
        memcpy(self->ptr + self->len, items, len * sizeof(T));                                     \
        self->len += len;                                                                          \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_pop(name##_t* self, T* out) {                                                      \
        assert(self != NULL);                                                                      \
        if (self->len == 0) return false;                                                          \
                                                                                                   \
        if (out != NULL) *out = self->ptr[self->len - 1];                                          \
        memset(self->ptr + self->len - 1, 0xDE, sizeof(T));                                        \
        self->len--;                                                                               \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_back(name##_t* self, T* out) {                                                     \
        assert(self != NULL);                                                                      \
        assert(out != NULL);                                                                       \
        if (self->len == 0) return false;                                                          \
                                                                                                   \
        *out = self->ptr[self->len - 1];                                                           \
        return true;                                                                               \
    }

#define DARRAY_IMPL(T) DARRAY_IMPL_RAW(T, darray_##T, const T*)
#endif
