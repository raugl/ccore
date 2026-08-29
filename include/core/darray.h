#pragma once
#include "common.h"
#include "allocator.h"

#define DARRAY_DECL(T) DARRAY_DECL_RAW(T, darray_##T, const T*)

#define DARRAY_DECL_RAW(T, Self, Ptr)                                                               \
    typedef struct Self {                                                                           \
        T*          data;                                                                           \
        allocator_t _allocator;                                                                     \
        u32         len, capacity;                                                                  \
    } Self;                                                                                         \
                                                                                                    \
    Self Self##_init_fixed(T* buffer, usize capacity);                                              \
    Self Self##_init_alloc(allocator_t allocator);                                                  \
    void Self##_destroy(Self* self);                                                                \
                                                                                                    \
    bool Self##_reserve(Self* self, usize capacity);                                                \
    bool Self##_reserve_exact(Self* self, usize capacity);                                          \
    bool Self##_reserve_spare(Self* self, usize count);                                             \
    bool Self##_shrink_to_fit(Self* self);                                                          \
    bool Self##_resize(Self* self, usize new_len);                                                  \
    void Self##_clear(Self* self);                                                                  \
                                                                                                    \
    bool Self##_pop(Self* self, T* out);                                                            \
    bool Self##_back(const Self* self, T* out);                                                     \
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
DARRAY_DECL(u32)
DARRAY_DECL(u64)
DARRAY_DECL(f32)

#define GENERICS_IMPLEMENTATION
#ifdef GENERICS_IMPLEMENTATION
#include "math.h"

// NOTE: This is just a common default, may not be accurate for any specific platform
#define CACHE_LINE_SIZE 64

#define DARRAY_IMPL(T) DARRAY_IMPL_RAW(T, darray_##T, const T*)

#define DARRAY_IMPL_RAW(T, Self, Ptr)                                                               \
    Self Self##_init_fixed(T* buffer, usize capacity) {                                             \
        assert(buffer != NULL);                                                                     \
        memset_undefined(buffer, capacity * sizeof(T));                                             \
                                                                                                    \
        return (Self) {                                                                             \
            .data       = buffer,                                                                   \
            .capacity   = (u32)capacity,                                                            \
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
        mem_free(self->_allocator, self->data, self->capacity);                                     \
        memset_destroyed(self, sizeof(Self));                                                         \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve(Self* self, usize new_capacity) {                                           \
        assert(self != NULL);                                                                       \
        if (self->capacity >= new_capacity) return true;                                            \
                                                                                                    \
        static const usize init_capacity = CACHE_LINE_SIZE / sizeof(T);                             \
        const usize growth = (new_capacity / 2 + init_capacity);                                    \
                                                                                                    \
        if (growth > UINT32_MAX - new_capacity) return false;                                       \
        return Self##_reserve_exact(self, new_capacity + growth);                                   \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve_spare(Self* self, usize count) {                                            \
        return Self##_reserve(self, self->len + count);                                             \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve_exact(Self* self, usize new_capacity) {                                     \
        assert(self != NULL);                                                                       \
        if (self->capacity >= new_capacity) return true;                                            \
                                                                                                    \
        T* new_ptr = mem_realloc(self->_allocator, self->data, self->capacity, new_capacity);       \
        if (new_ptr != NULL) {                                                                      \
            self->capacity = (u32)new_capacity;                                                     \
            self->data = new_ptr;                                                                   \
            return true;                                                                            \
        }                                                                                           \
        return false;                                                                               \
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
    bool Self##_shrink_to_fit(Self* self) {                                                         \
        assert(self != NULL);                                                                       \
        const usize new_capacity = self->len;                                                       \
                                                                                                    \
        T* new_ptr = mem_realloc(self->_allocator, self->data, self->capacity, new_capacity);       \
        if (new_ptr != NULL) {                                                                      \
            self->capacity = (u32)new_capacity;                                                     \
            self->data = new_ptr;                                                                   \
            return true;                                                                            \
        }                                                                                           \
        return false;                                                                               \
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
        if (count == 0) return true;                                                                \
        assert(items != NULL);                                                                      \
                                                                                                    \
        if (!Self##_reserve_spare(self, count)) return false;                                       \
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
        if (count == 0) return true;                                                                \
        assert(items != NULL);                                                                      \
                                                                                                    \
        if (!Self##_reserve_spare(self, count)) return false;                                       \
        memcpy(self->data + self->len, items, count * sizeof(T));                                   \
        self->len += (u32)count;                                                                    \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_add_many(Self* self, usize index, usize count) {                                    \
        assert(self != NULL);                                                                       \
        if (count == 0) return true;                                                                \
                                                                                                    \
        if (!Self##_reserve_spare(self, count)) return false;                                       \
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
    bool Self##_back(const Self* self, T* out) {                                                    \
        assert(self != NULL);                                                                       \
        assert(out != NULL);                                                                        \
        if (self->len == 0) return false;                                                           \
        *out = self->data[self->len - 1];                                                           \
        return true;                                                                                \
    }
#endif
