#pragma once
#include "allocator.h"
#include "common.h"
#include "slice.h"

#define DARRAY_DECL(T, name)                                                                       \
    typedef struct {                                                                               \
        T* ptr;                                                                                    \
        grow_policy_t policy;                                                                      \
        u32 len, capacity;                                                                         \
    } array_##name##_t;                                                                            \
                                                                                                   \
    array_##name##_t array_##name##_init_fixed(T* buffer, u32 len);                                \
    array_##name##_t array_##name##_init_arena(arena_t* arena, u32 capacity);                      \
    array_##name##_t array_##name##_init_alloc(allocator_t* alloc, u32 capacity);                  \
    slice_##name##_t array_##name##_slice(array_##name##_t self, u32 idx, u32 len);                \
    void array_##name##_release(array_##name##_t* self);                                           \
    void array_##name##_resize(array_##name##_t* self, u32 len);                                   \
    void array_##name##_ensure_capacity(array_##name##_t* self, u32 capacity);                     \
    void array_##name##_push(array_##name##_t* self, T data);                                      \
    T array_##name##_pop(array_##name##_t* self);                                                  \
    void array_##name##_insert(array_##name##_t* self, T data, u32 idx);                           \
    T array_##name##_remove(array_##name##_t* self, u32 idx);

DARRAY_DECL(u8, u8);
DARRAY_DECL(u16, u16);
DARRAY_DECL(u32, u32);
DARRAY_DECL(u64, u64);

DARRAY_DECL(i8, i8);
DARRAY_DECL(i16, i16);
DARRAY_DECL(i32, i32);
DARRAY_DECL(i64, i64);

DARRAY_DECL(f32, f32);
DARRAY_DECL(f64, f64);
DARRAY_DECL(bool, bool);

#ifdef GENERICS_IMPLEMENTATION

// TODO: Try to pull out as much duplicated computation into non-generics helpers

#define DARRAY_IMPL(T, name)                                                                       \
    array_##name##_t array_##name##_init_fixed(T* buffer, u32 len) {                               \
        assert(buffer != NULL);                                                                    \
                                                                                                   \
        return (array_##name##_t) {                                                                \
            .ptr = buffer,                                                                         \
            .capacity = len,                                                                       \
            .policy.kind = GROW_POLICY_FIXED,                                                      \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    array_##name##_t array_##name##_init_arena(arena_t* arena, u32 capacity) {                     \
        assert(arena != NULL);                                                                     \
                                                                                                   \
        array_##name##_t self = {                                                                  \
            .policy = {                                                                            \
                .arena = arena,                                                                    \
                .kind = GROW_POLICY_ARENA,                                                         \
            }                                                                                      \
        };                                                                                         \
        array_##name##_ensure_capacity(&self, capacity);                                           \
        return self;                                                                               \
    }                                                                                              \
                                                                                                   \
    array_##name##_t array_##name##_init_alloc(allocator_t* alloc, u32 capacity) {                 \
        assert(alloc != NULL);                                                                     \
                                                                                                   \
        array_##name##_t self = {                                                                  \
            .policy = {                                                                            \
                .alloc = alloc,                                                                    \
                .kind = GROW_POLICY_ALLOC,                                                         \
            }                                                                                      \
        };                                                                                         \
        array_##name##_ensure_capacity(&self, capacity);                                           \
        return self;                                                                               \
    }                                                                                              \
                                                                                                   \
    void array_##name##_release(array_##name##_t* self) {                                          \
        assert(self != NULL);                                                                      \
                                                                                                   \
        switch (self->policy.kind) {                                                               \
        case GROW_POLICY_ARENA:                                                                    \
            panic("attempted to release an arena allocated array");                                \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            alloc_free(self->policy.alloc, *self);                                                 \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            panic("attempted to release a fixed buffer array");                                    \
            break;                                                                                 \
        }                                                                                          \
        *self = (array_##name##_t) {};                                                             \
    }                                                                                              \
                                                                                                   \
    void array_##name##_ensure_capacity(array_##name##_t* self, u32 capacity) {                    \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->capacity < capacity) {                                                           \
            switch (self->policy.kind) {                                                           \
            case GROW_POLICY_ARENA:                                                                \
                self->ptr = arena_realloc(self->policy.arena, *self, capacity);                    \
                break;                                                                             \
            case GROW_POLICY_ALLOC:                                                                \
                self->ptr = alloc_realloc(self->policy.alloc, *self, capacity);                    \
                break;                                                                             \
            case GROW_POLICY_FIXED:                                                                \
                panic("fixed buffer array ran out of memory");                                     \
                break;                                                                             \
            }                                                                                      \
            self->capacity = capacity;                                                             \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    slice_##name##_t array_##name##_slice(array_##name##_t self, u32 idx, u32 len) {               \
        assert(idx < self.len);                                                                    \
        assert(idx + len <= self.len);                                                             \
                                                                                                   \
        return (slice_##name##_t) {                                                                \
            .ptr = self.ptr + idx,                                                                 \
            .len = (len > 0) ? len : self.len,                                                     \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    void array_##name##_resize(array_##name##_t* self, u32 len) {                                  \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->capacity < len) {                                                                \
            array_##name##_ensure_capacity(self, len);                                             \
        }                                                                                          \
        self->len = len;                                                                           \
    }                                                                                              \
                                                                                                   \
    void array_##name##_push(array_##name##_t* self, T data) {                                     \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->len == self->capacity) {                                                         \
            u32 new_capacity = (self->capacity == 0) ? 8 : self->capacity + (self->capacity >> 1); \
            array_##name##_ensure_capacity(self, new_capacity);                                    \
        }                                                                                          \
        self->ptr[self->len++] = data;                                                             \
    }                                                                                              \
                                                                                                   \
    T array_##name##_pop(array_##name##_t* self) {                                                 \
        assert(self != NULL);                                                                      \
        assert(self->len > 0);                                                                     \
        assert(self->ptr != NULL);                                                                 \
                                                                                                   \
        return self->ptr[--self->len];                                                             \
    }                                                                                              \
                                                                                                   \
    void array_##name##_insert(array_##name##_t* self, T data, u32 idx) {                          \
        assert(self != NULL);                                                                      \
        assert(idx <= self->len);                                                                  \
                                                                                                   \
        if (self->len == self->capacity) {                                                         \
            u32 new_capacity = (self->capacity == 0) ? 8 : self->capacity + (self->capacity >> 1); \
            array_##name##_ensure_capacity(self, new_capacity);                                    \
        }                                                                                          \
        memmove(self->ptr + idx + 1, self->ptr + idx, self->len - idx);                            \
        self->ptr[idx] = data;                                                                     \
    }                                                                                              \
                                                                                                   \
    T array_##name##_remove(array_##name##_t* self, u32 idx) {                                     \
        assert(self != NULL);                                                                      \
        assert(idx < self->len);                                                                   \
                                                                                                   \
        T item = self->ptr[idx];                                                                   \
        memmove(self->ptr + idx, self->ptr + idx + 1, self->len - idx - 1);                        \
        return item;                                                                               \
    }
#endif
