#pragma once
#include "allocator.h"
#include "common.h"
// FIXME: When initializing with a capacity, the arena or allocator might fail, resulting in a
// invalid state. I don't see a good way of returning that error without destroying my API.

#define PQUEUE_DECL_RAW(T, name)                                                                   \
    typedef struct {                                                                               \
        T* ptr_;                                                                                   \
        grow_policy_t grow_;                                                                       \
        u32 len, capacity;                                                                         \
    } name##_t;                                                                                    \
                                                                                                   \
    name##_t name##_init_fixed(T* buffer, usize capacity);                                         \
    name##_t name##_init_arena(arena_t* arena, usize capacity);                                    \
    name##_t name##_init_alloc(allocator_t* alloc, usize capacity);                                \
    void name##_release(name##_t* self);                                                           \
                                                                                                   \
    name##_t name##_from_fixed(T* buffer, usize capacity);                                         \
    bool name##_ensure_capacity(name##_t* self, usize capacity);                                   \
    bool name##_push(name##_t* self, T item);                                                      \
    bool name##_pop(name##_t* self, T* out);

#define PQUEUE_DECL(T) PQUEUE_DECL_RAW(T, pqueue_##T)

// TODO: This should return an optional, move it to the template
#define pqueue_peek(self) array_at((self), 0)

PQUEUE_DECL(u32);

#define MIN_CAPACITY 8

#define PQUEUE_IMPL_RAW(T, name, cmp_fn)                                                           \
    static void sift_up_##name(T* arr, usize node) {                                               \
        usize parent = (node - 1) / 2;                                                             \
        while (node != 0 && cmp_fn(arr[node], arr[parent])) {                                      \
            T tmp = arr[node];                                                                     \
            arr[node] = arr[parent];                                                               \
            arr[parent] = tmp;                                                                     \
            node = parent;                                                                         \
            parent = (node - 1) / 2;                                                               \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    static void sift_down_##name(T* arr, usize len, usize node) {                                  \
        while (true) {                                                                             \
            usize child = node * 2 + 1; /* left child */                                           \
            if (child >= len) break;                                                               \
            if (child + 1 < len && cmp_fn(arr[child], arr[child + 1])) child++;                    \
                                                                                                   \
            if (cmp_fn(arr[node], arr[child])) {                                                   \
                T tmp = arr[node];                                                                 \
                arr[node] = arr[child];                                                            \
                arr[child] = tmp;                                                                  \
                node = child;                                                                      \
            } else {                                                                               \
                break;                                                                             \
            }                                                                                      \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_fixed(T* buffer, usize capacity) {                                        \
        assert(buffer != NULL);                                                                    \
        return (name##_t) {                                                                        \
            .ptr_ = buffer,                                                                        \
            .capacity = capacity,                                                                  \
            .grow_.kind = GROW_POLICY_FIXED,                                                       \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_arena(arena_t* arena, usize capacity) {                                   \
        assert(arena != NULL);                                                                     \
        return (name##_t) {                                                                        \
            .ptr_ = arena_alloc(arena, capacity * sizeof(T)),                                      \
            .capacity = capacity,                                                                  \
            .grow_.arena = arena,                                                                  \
            .grow_.kind = GROW_POLICY_ARENA,                                                       \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_alloc(allocator_t* alloc, usize capacity) {                               \
        assert(alloc != NULL);                                                                     \
        return (name##_t) {                                                                        \
            .ptr_ = alloc_alloc(alloc, capacity * sizeof(T)),                                      \
            .capacity = capacity,                                                                  \
            .grow_.alloc = alloc,                                                                  \
            .grow_.kind = GROW_POLICY_ALLOC,                                                       \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_from_fixed(T* buffer, usize capacity) {                                        \
        assert(buffer != NULL);                                                                    \
        for (usize i = capacity / 2; i-- > 0;) {                                                   \
            sift_down_##name(buffer, capacity, i);                                                 \
        }                                                                                          \
        return (name##_t) {                                                                        \
            .ptr_ = buffer,                                                                        \
            .capacity = capacity,                                                                  \
            .grow_.kind = GROW_POLICY_FIXED,                                                       \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    void name##_release(name##_t* self) {                                                          \
        assert(self != NULL);                                                                      \
                                                                                                   \
        switch (self->grow_.kind) {                                                                \
        case GROW_POLICY_ARENA:                                                                    \
            log_error("attempted to release an arena allocated pqueue");                           \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            alloc_free(self->grow_.alloc, *self);                                                  \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            log_error("attempted to release a fixed buffer pdeque");                               \
            break;                                                                                 \
        }                                                                                          \
        memset(self, 0xDE, sizeof(name##_t));                                                      \
    }                                                                                              \
                                                                                                   \
    bool name##_ensure_capacity(name##_t* self, usize new_capacity) {                              \
        assert(self != NULL);                                                                      \
        if (self->capacity >= new_capacity) return true;                                           \
        new_capacity = next_pow2_u64(max_usize(MIN_CAPACITY, new_capacity));                       \
        T* new_ptr;                                                                                \
                                                                                                   \
        switch (self->grow_.kind) {                                                                \
        case GROW_POLICY_ARENA:                                                                    \
            new_ptr = arena_realloc(self->grow_.arena, *self, new_capacity);                       \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            new_ptr = alloc_realloc(self->grow_.alloc, *self, new_capacity);                       \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            panic("fixed buffer deque cannot allocate new buffer");                                \
            break;                                                                                 \
        }                                                                                          \
        if (new_ptr == NULL) return false;                                                         \
        self->capacity = new_capacity;                                                             \
        self->ptr_ = new_ptr;                                                                      \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_push(name##_t* self, T item) {                                                     \
        assert(self != NULL);                                                                      \
        if (self->len == self->capacity) {                                                         \
            if (!name##_ensure_capacity(self, self->capacity + 1)) return false;                   \
        }                                                                                          \
        self->ptr_[self->len] = item;                                                              \
        sift_up_##name(self->ptr_, self->len);                                                     \
        self->len++;                                                                               \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_pop(name##_t* self, T* out) {                                                      \
        assert(self != NULL);                                                                      \
        assert(self->ptr_ != NULL);                                                                \
        if (self->len == 0) {                                                                      \
            return false;                                                                          \
        }                                                                                          \
        if (out != NULL) *out = self->ptr_[0];                                                     \
        self->ptr_[0] = self->ptr_[--self->len];                                                   \
        sift_down_##name(self->ptr_, self->len, 0);                                                \
        return true;                                                                               \
    }

#define PQUEUE_IMPL(T, cmp_fn) PQUEUE_IMPL_RAW(T, pqueue_##T, cmp_fn)
