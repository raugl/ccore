#pragma once
#include "allocator.h"
#include "common.h"

#define DARRAY_DECL(T) DARRAY_DECL_RAW(T, darray_##T, const T*)

#define DARRAY_DECL_RAW(T, Self, Ptr)                                                               \
    typedef struct {                                                                                \
        T* ptr;                                                                                     \
        allocator_t allocator;                                                                      \
        u32 len, capacity;                                                                          \
    } Self;                                                                                         \
                                                                                                    \
    Self Self##_init_fixed(T* buffer, usize capacity);                                              \
    Self Self##_init_alloc(allocator_t allocator);                                                  \
    void Self##_release(Self* self);                                                                \
                                                                                                    \
    bool Self##_ensure_capacity(Self* self, usize new_capacity);                                    \
    bool Self##_resize(Self* self, usize new_len);                                                  \
    void Self##_shrink_to_fit(Self* self);                                                          \
    void Self##_clear(Self* self);                                                                  \
                                                                                                    \
    bool Self##_push(Self* self, T item);                                                           \
    bool Self##_pop(Self* self, T* out);                                                            \
    bool Self##_back(Self* self, T* out);                                                           \
    bool Self##_insert(Self* self, usize idx, T item);                                              \
    T Self##_remove(Self* self, usize idx);                                                         \
    T Self##_swap_remove(Self* self, usize idx);                                                    \
    bool Self##_push_many(Self* self, Ptr items, usize len);                                        \
    bool Self##_insert_many(Self* self, usize idx, Ptr items, usize len);                           \
    void Self##_remove_many(Self* self, usize idx, usize len);                                      \
    void Self##_swap_remove_many(Self* self, usize idx, usize len);

DARRAY_DECL_RAW(u8, darray_u8, const void*)
DARRAY_DECL(u32)
DARRAY_DECL(u64)
DARRAY_DECL(f32)

#ifdef GENERICS_IMPLEMENTATION
#include "math.h"
#define MIN_CAPACITY 8

#define DARRAY_IMPL(T) DARRAY_IMPL_RAW(T, darray_##T, const T*)

#define DARRAY_IMPL_RAW(T, Self, Ptr)                                                               \
    Self Self##_init_fixed(T* buffer, usize capacity) {                                             \
        assert(buffer != NULL);                                                                     \
        return (Self) {                                                                             \
            .ptr = buffer,                                                                          \
            .capacity = (u32)capacity,                                                              \
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
        mem_free(self->allocator, self->ptr, self->capacity);                                       \
        memset(self, 0xDE, sizeof(Self));                                                           \
    }                                                                                               \
                                                                                                    \
    /* FIXME: somehow try to detect u32 overflow for the new_capacity and return out of memory */   \
    bool Self##_ensure_capacity(Self* self, usize new_capacity) {                                   \
        assert(self != NULL);                                                                       \
        if (self->capacity >= new_capacity) return true;                                            \
                                                                                                    \
        new_capacity = next_pow2_u64(max_usize(MIN_CAPACITY, new_capacity));                        \
        T* new_ptr = mem_realloc(self->allocator, self->ptr, self->capacity, new_capacity);         \
        if (new_ptr == NULL) return false;                                                          \
                                                                                                    \
        self->capacity = (u32)new_capacity;                                                         \
        self->ptr = new_ptr;                                                                        \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_resize(Self* self, usize new_len) {                                                 \
        assert(self != NULL);                                                                       \
                                                                                                    \
        if (self->capacity < new_len) {                                                             \
            if (!Self##_ensure_capacity(self, new_len)) return false;                               \
        } else if (self->len > new_len) {                                                           \
            memset(self->ptr + new_len, 0xDE, (self->len - new_len) * sizeof(T));                   \
        }                                                                                           \
        self->len = (u32)new_len;                                                                   \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    void Self##_shrink_to_fit(Self* self) {                                                         \
        assert(self != NULL);                                                                       \
        usize new_capacity = self->len;                                                             \
        if (self->len <= self->capacity / 4) new_capacity = next_pow2_u64(self->len);               \
        if (self->len >= self->capacity) return;                                                    \
                                                                                                    \
        if (!mem_resize(self->allocator, self->ptr, self->capacity, new_capacity)) {                \
            T* new_ptr = mem_realloc(self->allocator, self->ptr, self->capacity, new_capacity);     \
            if (new_ptr != NULL) {                                                                  \
                self->capacity = (u32)new_capacity;                                                 \
                self->ptr = new_ptr;                                                                \
            }                                                                                       \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    void Self##_clear(Self* self) {                                                                 \
        assert(self != NULL);                                                                       \
        memset(self->ptr, 0xDE, self->len * sizeof(T));                                             \
        self->len = 0;                                                                              \
    }                                                                                               \
                                                                                                    \
    bool Self##_insert(Self* self, usize idx, T item) {                                             \
        return Self##_insert_many(self, idx, &item, 1);                                             \
    }                                                                                               \
                                                                                                    \
    bool Self##_insert_many(Self* self, usize idx, Ptr items, usize len) {                          \
        assert(self != NULL);                                                                       \
        assert(idx <= self->len);                                                                   \
        if (len == 0) return true;                                                                  \
        assert(items != NULL);                                                                      \
                                                                                                    \
        if (self->len + len >= self->capacity) {                                                    \
            if (!Self##_ensure_capacity(self, self->len + len)) return false;                       \
        }                                                                                           \
        memmove(self->ptr + idx + len, self->ptr + idx, (self->len - idx) * sizeof(T));             \
        memcpy(self->ptr + idx, items, len * sizeof(T));                                            \
        self->len += (u32)len;                                                                      \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    T Self##_remove(Self* self, usize idx) {                                                        \
        assert(self != NULL);                                                                       \
        assert(idx < self->len);                                                                    \
                                                                                                    \
        T old_val = self->ptr[idx];                                                                 \
        Self##_remove_many(self, idx, 1);                                                           \
        return old_val;                                                                             \
    }                                                                                               \
                                                                                                    \
    void Self##_remove_many(Self* self, usize idx, usize len) {                                     \
        assert(self != NULL);                                                                       \
        assert(idx + len <= self->len);                                                             \
                                                                                                    \
        memmove(self->ptr + idx, self->ptr + idx + len, self->len - idx * sizeof(T));               \
        memset(self->ptr + (self->len - len), 0xDE, len * sizeof(T));                               \
        self->len -= (u32)len;                                                                      \
    }                                                                                               \
                                                                                                    \
    T Self##_swap_remove(Self* self, usize idx) {                                                   \
        assert(self != NULL);                                                                       \
        assert(idx < self->len);                                                                    \
                                                                                                    \
        T old_val = self->ptr[idx];                                                                 \
        Self##_swap_remove_many(self, idx, 1);                                                      \
        return old_val;                                                                             \
    }                                                                                               \
                                                                                                    \
    void Self##_swap_remove_many(Self* self, usize idx, usize len) {                                \
        assert(self != NULL);                                                                       \
        assert(idx + len <= self->len);                                                             \
                                                                                                    \
        memmove(self->ptr + idx, self->ptr + (self->len - len), len * sizeof(T));                   \
        memset(self->ptr + (self->len - len), 0xDE, len * sizeof(T));                               \
        self->len -= (u32)len;                                                                      \
    }                                                                                               \
                                                                                                    \
    bool Self##_push(Self* self, T item) {                                                          \
        return Self##_push_many(self, &item, 1);                                                    \
    }                                                                                               \
                                                                                                    \
    bool Self##_push_many(Self* self, Ptr items, usize len) {                                       \
        assert(self != NULL);                                                                       \
        if (len == 0) return true;                                                                  \
        assert(items != NULL);                                                                      \
                                                                                                    \
        if (self->len + len >= self->capacity) {                                                    \
            if (!Self##_ensure_capacity(self, self->len + len)) return false;                       \
        }                                                                                           \
        memcpy(self->ptr + self->len, items, len * sizeof(T));                                      \
        self->len += (u32)len;                                                                      \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_pop(Self* self, T* out) {                                                           \
        assert(self != NULL);                                                                       \
        if (self->len == 0) return false;                                                           \
                                                                                                    \
        if (out != NULL) *out = self->ptr[self->len - 1];                                           \
        memset(self->ptr + self->len - 1, 0xDE, sizeof(T));                                         \
        self->len--;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_back(Self* self, T* out) {                                                          \
        assert(self != NULL);                                                                       \
        assert(out != NULL);                                                                        \
        if (self->len == 0) return false;                                                           \
                                                                                                    \
        *out = self->ptr[self->len - 1];                                                            \
        return true;                                                                                \
    }
#endif
