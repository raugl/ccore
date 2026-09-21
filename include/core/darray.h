#pragma once
#include "common.h"
#include "arena_allocator.h"

typedef struct raw_darray {
    arena_t* _arena;
    u8*      data;
    u32      len, capacity;
} raw_darray;

#define DARRAY_DECL(T) DARRAY_DECL_RAW(T, darray_##T, const T*)

#define DARRAY_DECL_RAW(T, Self, Ptr)                                                               \
    typedef struct Self {                                                                           \
        arena_t _arena;                                                                             \
        T*      data;                                                                               \
        u32     len, capacity;                                                                      \
    } Self;                                                                                         \
                                                                                                    \
    Self Self##_init(arena_t* arena);                                                               \
    bool Self##_reserve(Self* self, usize capacity);                                                \
    bool Self##_reserve_exact(Self* self, usize capacity);                                          \
    bool Self##_reserve_spare(Self* self, usize count);                                             \
    bool Self##_resize(Self* self, usize new_len);                                                  \
    void Self##_clear(Self* self);                                                                  \
                                                                                                    \
    bool Self##_pop(Self* self, T* out);                                                            \
    bool Self##_back(Self self, T** out);                                                           \
    bool Self##_append(Self* self, T item);                                                         \
    bool Self##_insert(Self* self, usize index, T item);                                            \
    T Self##_remove(Self* self, usize index);                                                       \
    T Self##_swap_remove(Self* self, usize index);                                                  \
    bool Self##_add_many(Self* self, usize index, usize count);                                     \
    bool Self##_append_many(Self* self, Ptr items, usize count);                                    \
    bool Self##_insert_many(Self* self, usize index, Ptr items, usize count);                       \
    void Self##_remove_many(Self* self, usize index, usize count);                                  \
    void Self##_swap_remove_many(Self* self, usize index, usize count);

DARRAY_DECL_RAW(u8, darray_u8, const void*)

#ifdef GENERICS_IMPLEMENTATION
#include "math.h"

#define DARRAY_IMPL(T) DARRAY_IMPL_RAW(T, darray_##T, const T*)

#define DARRAY_IMPL_RAW(T, Self, Ptr)                                                               \
    Self Self##_init(arena_t* arena) {                                                              \
        assert(arena != NULL);                                                                      \
        return (Self) { ._arena = arena };                                                          \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve(Self* self, usize new_capacity) {                                           \
        assert(self != NULL);                                                                       \
        if (new_capacity <= self->capacity) return true;                                            \
                                                                                                    \
        new_capacity = next_container_capacity(T, self->capacity, container_capacity);              \
        if (new_capacity > UINT32_MAX) return false;                                                \
        return Self##_reserve_exact(self, new_capacity);                                            \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve_exact(Self* self, usize new_capacity) {                                     \
        assert(self != NULL);                                                                       \
        if (self->capacity >= new_capacity) return true;                                            \
                                                                                                    \
        T* new_ptr = arena_realloc(self->_arena, self->data, self->capacity, new_capacity);         \
        if (new_ptr == NULL) return false;                                                          \
                                                                                                    \
        self->capacity = (u32)new_capacity;                                                         \
        self->data = new_ptr;                                                                       \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve_spare(Self* self, usize count) {                                            \
        assert(self != NULL);                                                                       \
        return Self##_reserve(self, self->len + count);                                             \
    }                                                                                               \
                                                                                                    \
    bool Self##_resize(Self* self, usize new_len) {                                                 \
        assert(self != NULL);                                                                       \
        if (!Self##_reserve(self, new_len)) return false;                                           \
                                                                                                    \
        if (self->len > new_len) {                                                                  \
            memset_destroyed(self->data + new_len, (self->len - new_len) * sizeof(T));              \
        }                                                                                           \
        self->len = (u32)new_len;                                                                   \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    void Self##_clear(Self* self) {                                                                 \
        assert(self != NULL);                                                                       \
        memset_destroyed(self->data, self->len * sizeof(T));                                        \
        self->len = 0;                                                                              \
    }                                                                                               \
                                                                                                    \
    bool Self##_insert(Self* self, usize index, T item) {                                           \
        return Self##_insert_many(self, index, &item, 1);                                           \
    }                                                                                               \
                                                                                                    \
    bool Self##_insert_many(Self* self, usize index, Ptr items, usize count) {                      \
        assert(self != NULL);                                                                       \
        assert(index <= self->len);                                                                 \
        assert((count == 0) || (items != NULL));                                                    \
                                                                                                    \
        if (count == 0) return true;                                                                \
        if (!Self##_reserve_spare(self, count)) return false;                                       \
                                                                                                    \
        memmove(self->data + index + count, self->data + index, (self->len - index) * sizeof(T));   \
        memcpy(self->data + index, items, count * sizeof(T));                                       \
        self->len += (u32)count;                                                                    \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    T Self##_remove(Self* self, usize index) {                                                      \
        assert(self != NULL);                                                                       \
        assert(index < self->len);                                                                  \
                                                                                                    \
        T old_val = self->data[index];                                                              \
        Self##_remove_many(self, index, 1);                                                         \
        return old_val;                                                                             \
    }                                                                                               \
                                                                                                    \
    void Self##_remove_many(Self* self, usize index, usize count) {                                 \
        assert(self != NULL);                                                                       \
        assert(index + count <= self->len);                                                         \
                                                                                                    \
        const usize end_index = index + count;                                                      \
        memmove(self->data + index, self->data + end_index, self->len - end_index * sizeof(T));     \
        memset_destroyed(self->data + (self->len - count), count * sizeof(T));                      \
        self->len -= (u32)count;                                                                    \
    }                                                                                               \
                                                                                                    \
    T Self##_swap_remove(Self* self, usize index) {                                                 \
        assert(self != NULL);                                                                       \
        assert(index < self->len);                                                                  \
                                                                                                    \
        T old_val = self->data[index];                                                              \
        Self##_swap_remove_many(self, index, 1);                                                    \
        return old_val;                                                                             \
    }                                                                                               \
                                                                                                    \
    void Self##_swap_remove_many(Self* self, usize index, usize count) {                            \
        assert(self != NULL);                                                                       \
        assert(index + count <= self->len);                                                         \
                                                                                                    \
        memmove(self->data + index, self->data + (self->len - count), count * sizeof(T));           \
        memset_destroyed(self->data + (self->len - count), count * sizeof(T));                      \
        self->len -= (u32)count;                                                                    \
    }                                                                                               \
                                                                                                    \
    bool Self##_append(Self* self, T item) {                                                        \
        return Self##_append_many(self, &item, 1);                                                  \
    }                                                                                               \
                                                                                                    \
    bool Self##_append_many(Self* self, Ptr items, usize count) {                                   \
        assert(self != NULL);                                                                       \
        assert((count == 0) || (items != NULL));                                                    \
                                                                                                    \
        if (count == 0) return true;                                                                \
        if (!Self##_reserve_spare(self, count)) return false;                                       \
                                                                                                    \
        memcpy(self->data + self->len, items, count * sizeof(T));                                   \
        self->len += (u32)count;                                                                    \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_add_many(Self* self, usize index, usize count) {                                    \
        assert(self != NULL);                                                                       \
        assert(index <= self->len);                                                                 \
                                                                                                    \
        if (count == 0) return true;                                                                \
        if (!Self##_reserve_spare(self, count)) return false;                                       \
                                                                                                    \
        memmove(self->data + index + count, self->data + index, (self->len - index) * sizeof(T));   \
        memset_undefined(self->data + index, count * sizeof(T));                                    \
        self->len += (u32)count;                                                                    \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_pop(Self* self, T* out) {                                                           \
        assert(self != NULL);                                                                       \
        if (self->len == 0) return false;                                                           \
        if (out) *out = self->data[--self->len];                                                    \
        memset_destroyed(self->data + self->len, sizeof(T));                                        \
        self->len--;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_back(Self self, T** out) {                                                          \
        assert(out != NULL);                                                                        \
        if (self.len == 0) return false;                                                            \
        *out = &self.data[self.len - 1];                                                            \
        return true;                                                                                \
    }
#endif
