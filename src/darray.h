#pragma once
#include "allocator.h"
#include "common.h"
#include "slice.h"

#define DARRAY_RAW_DECL(T, name, S)                                                                \
    typedef struct {                                                                               \
        T* ptr;                                                                                    \
        grow_policy_t grow;                                                                        \
        u32 len, capacity;                                                                         \
    } name##_t;                                                                                    \
                                                                                                   \
    name##_t name##_init_fixed(T* buffer, usize capacity);                                         \
    name##_t name##_init_arena(arena_t* arena, usize capacity);                                    \
    name##_t name##_init_alloc(allocator_t* alloc, usize capacity);                                \
    S name##_slice(name##_t self, usize idx, usize len);                                           \
    void name##_release(name##_t* self);                                                           \
    void name##_resize(name##_t* self, usize len);                                                 \
    void name##_ensure_capacity(name##_t* self, usize capacity);                                   \
    void name##_push(name##_t* self, T data);                                                      \
    T name##_pop(name##_t* self);                                                                  \
    void name##_insert(name##_t* self, T data, usize idx);                                         \
    T name##_remove(name##_t* self, usize idx);

#define DARRAY_DECL(T) DARRAY_RAW_DECL(T, array_##T, slice_##T##_t)

DARRAY_DECL(u8);
DARRAY_DECL(u16);
DARRAY_DECL(u32);
DARRAY_DECL(u64);

DARRAY_DECL(i8);
DARRAY_DECL(i16);
DARRAY_DECL(i32);
DARRAY_DECL(i64);

DARRAY_DECL(f32);
DARRAY_DECL(f64);
DARRAY_DECL(bool);

#ifdef GENERICS_IMPLEMENTATION
#include "math.h"

// TODO: Try to pull out as much duplicated computation into non-generics helpers

#define MIN_CAPACITY 8

#define DARRAY_RAW_IMPL(T, name, S)                                                                \
    name##_t name##_init_fixed(T* buffer, usize capacity) {                                        \
        assert(buffer != NULL);                                                                    \
        assert((uintptr_t)buffer % sizeof(max_align_t) == 0);                                      \
                                                                                                   \
        return (name##_t) {                                                                        \
            .ptr = buffer,                                                                         \
            .capacity = capacity,                                                                  \
            .grow.kind = GROW_POLICY_FIXED,                                                        \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_arena(arena_t* arena, usize capacity) {                                   \
        assert(arena != NULL);                                                                     \
                                                                                                   \
        name##_t self = {                                                                          \
            .grow.arena = arena,                                                                   \
            .grow.kind = GROW_POLICY_ARENA,                                                        \
        };                                                                                         \
        name##_ensure_capacity(&self, capacity);                                                   \
        return self;                                                                               \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_alloc(allocator_t* alloc, usize capacity) {                               \
        assert(alloc != NULL);                                                                     \
                                                                                                   \
        name##_t self = {                                                                          \
            .grow.alloc = alloc,                                                                   \
            .grow.kind = GROW_POLICY_ALLOC,                                                        \
        };                                                                                         \
        name##_ensure_capacity(&self, capacity);                                                   \
        return self;                                                                               \
    }                                                                                              \
                                                                                                   \
    void name##_release(name##_t* self) {                                                          \
        assert(self != NULL);                                                                      \
                                                                                                   \
        switch (self->grow.kind) {                                                                 \
        case GROW_POLICY_ARENA:                                                                    \
            log_error("attempted to release an arena allocated array");                            \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            alloc_free(self->grow.alloc, *self);                                                   \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            log_error("attempted to release a fixed buffer array");                                \
            break;                                                                                 \
        }                                                                                          \
        memset(self, 0, sizeof(name##_t));                                                         \
    }                                                                                              \
                                                                                                   \
    void name##_ensure_capacity(name##_t* self, usize capacity) {                                  \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->capacity < capacity) {                                                           \
            switch (self->grow.kind) {                                                             \
            case GROW_POLICY_ARENA:                                                                \
                self->ptr = arena_realloc(self->grow.arena, *self, capacity);                      \
                break;                                                                             \
            case GROW_POLICY_ALLOC:                                                                \
                self->ptr = alloc_realloc(self->grow.alloc, *self, capacity);                      \
                break;                                                                             \
            case GROW_POLICY_FIXED:                                                                \
                panic("fixed buffer array ran out of memory");                                     \
                break;                                                                             \
            }                                                                                      \
            self->capacity = capacity;                                                             \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    S name##_slice(name##_t self, usize idx, usize len) {                                          \
        assert(idx < self.len);                                                                    \
        assert(idx + len <= self.len);                                                             \
                                                                                                   \
        return (S) {                                                                               \
            .ptr = self.ptr + idx,                                                                 \
            .len = (len > 0) ? len : self.len,                                                     \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    void name##_resize(name##_t* self, usize len) {                                                \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->capacity < len) {                                                                \
            name##_ensure_capacity(self, len);                                                     \
        } else if (self->len > len) {                                                              \
            memset(self->ptr + len, 0, (self->len - len) * sizeof(T));                             \
        }                                                                                          \
        self->len = len;                                                                           \
    }                                                                                              \
                                                                                                   \
    void name##_push(name##_t* self, T data) {                                                     \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->len == self->capacity) {                                                         \
            u32 new_capacity = max_u32(MIN_CAPACITY, self->capacity + self->capacity / 2);         \
            name##_ensure_capacity(self, new_capacity);                                            \
        }                                                                                          \
        self->ptr[self->len++] = data;                                                             \
    }                                                                                              \
                                                                                                   \
    T name##_pop(name##_t* self) {                                                                 \
        assert(self != NULL);                                                                      \
        assert(self->len > 0);                                                                     \
        assert(self->ptr != NULL);                                                                 \
                                                                                                   \
        return self->ptr[--self->len];                                                             \
    }                                                                                              \
                                                                                                   \
    void name##_insert(name##_t* self, T data, usize idx) {                                        \
        assert(self != NULL);                                                                      \
        assert(idx <= self->len);                                                                  \
                                                                                                   \
        if (self->len == self->capacity) {                                                         \
            u32 new_capacity = max_u32(MIN_CAPACITY, self->capacity + self->capacity / 2);         \
            name##_ensure_capacity(self, new_capacity);                                            \
        }                                                                                          \
        memmove(self->ptr + idx + 1, self->ptr + idx, self->len - idx);                            \
        self->ptr[idx] = data;                                                                     \
    }                                                                                              \
                                                                                                   \
    T name##_remove(name##_t* self, usize idx) {                                                   \
        assert(self != NULL);                                                                      \
        assert(idx < self->len);                                                                   \
                                                                                                   \
        T item = self->ptr[idx];                                                                   \
        memmove(self->ptr + idx, self->ptr + idx + 1, self->len - idx - 1);                        \
        return item;                                                                               \
    }

#define DARRAY_IMPL(T) DARRAY_RAW_IMPL(T, array_##T, slice_##T##_t)
#endif
