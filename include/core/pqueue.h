#pragma once
#include "allocator.h"
#include "common.h"

#define PQUEUE_DECL(T) PQUEUE_DECL_RAW(T, pqueue_##T)

#define PQUEUE_DECL_RAW(T, Self)                                                                    \
    typedef struct {                                                                                \
        T* ptr_;                                                                                    \
        allocator_t allocator;                                                                      \
        u32 len, capacity;                                                                          \
    } Self;                                                                                         \
                                                                                                    \
    Self Self##_init_fixed(T* buffer, usize capacity);                                              \
    Self Self##_init_alloc(allocator_t allocator);                                                  \
    void Self##_release(Self* self);                                                                \
                                                                                                    \
    Self Self##_init_fixed(T* buffer, usize capacity);                                              \
    bool Self##_ensure_capacity(Self* self, usize capacity);                                        \
    bool Self##_push(Self* self, T item);                                                           \
    bool Self##_peek(Self* self, T* out);                                                           \
    bool Self##_pop(Self* self, T* out);

PQUEUE_DECL(u8)
PQUEUE_DECL(u32)
PQUEUE_DECL(u64)
PQUEUE_DECL(f32)

#ifdef GENERICS_IMPLEMENTATION
#define MIN_CAPACITY 8

#define PQUEUE_IMPL(T, cmp_fn) PQUEUE_IMPL_RAW(T, pqueue_##T, cmp_fn)

#define PQUEUE_IMPL_RAW(T, Self, cmp_fn)                                                            \
    static void sift_up_##Self(T* arr, usize node) {                                                \
        usize parent = (node - 1) / 2;                                                              \
        while (node != 0 && cmp_fn(arr[node], arr[parent])) {                                       \
            T tmp = arr[node];                                                                      \
            arr[node] = arr[parent];                                                                \
            arr[parent] = tmp;                                                                      \
            node = parent;                                                                          \
            parent = (node - 1) / 2;                                                                \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static void sift_down_##Self(T* arr, usize len, usize node) {                                   \
        while (true) {                                                                              \
            usize child = node * 2 + 1; /* left child */                                            \
            if (child >= len) break;                                                                \
            if (child + 1 < len && cmp_fn(arr[child], arr[child + 1])) child++;                     \
                                                                                                    \
            if (cmp_fn(arr[node], arr[child])) {                                                    \
                T tmp = arr[node];                                                                  \
                arr[node] = arr[child];                                                             \
                arr[child] = tmp;                                                                   \
                node = child;                                                                       \
            } else {                                                                                \
                break;                                                                              \
            }                                                                                       \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    Self Self##_init_fixed(T* buffer, usize capacity) {                                             \
        assert(buffer != NULL);                                                                     \
        return (Self) {                                                                             \
            .ptr_ = buffer,                                                                         \
            .capacity = (u32)capacity,                                                              \
            .allocator = allocator_init_null(),                                                     \
        };                                                                                          \
    }                                                                                               \
                                                                                                    \
    Self Self##_init_alloc(allocator_t allocator) {                                                 \
        assert(allocator.vtable != NULL);                                                           \
        return (Self) { .allocator = allocator };                                                   \
    }                                                                                               \
                                                                                                    \
    Self Self##_init_fixed(T* buffer, usize capacity) {                                             \
        assert(buffer != NULL);                                                                     \
        for (usize i = capacity / 2; i-- > 0;) {                                                    \
            sift_down_##Self(buffer, capacity, i);                                                  \
        }                                                                                           \
        /* TODO: // .len = capacity,*/                                                              \
        return (Self) {                                                                             \
            .ptr_ = buffer,                                                                         \
            .capacity = (u32)capacity,                                                              \
            .allocator = allocator_init_null(),                                                     \
        };                                                                                          \
    }                                                                                               \
                                                                                                    \
    void Self##_release(Self* self) {                                                               \
        assert(self != NULL);                                                                       \
        mem_free(self->allocator, self->ptr_, self->capacity);                                      \
        memset(self, 0xDE, sizeof(Self));                                                           \
    }                                                                                               \
                                                                                                    \
    bool Self##_ensure_capacity(Self* self, usize new_capacity) {                                   \
        assert(self != NULL);                                                                       \
        if (self->capacity >= new_capacity) return true;                                            \
                                                                                                    \
        new_capacity = next_pow2_u64(max_usize(MIN_CAPACITY, new_capacity));                        \
        T* new_ptr = mem_realloc(self->allocator, self->ptr_, self->capacity, new_capacity);        \
        if (new_ptr == NULL) return false;                                                          \
                                                                                                    \
        self->capacity = (u32)new_capacity;                                                         \
        self->ptr_ = new_ptr;                                                                       \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_push(Self* self, T item) {                                                          \
        assert(self != NULL);                                                                       \
        if (self->len == self->capacity) {                                                          \
            if (!Self##_ensure_capacity(self, self->capacity + 1)) return false;                    \
        }                                                                                           \
        self->ptr_[self->len] = item;                                                               \
        sift_up_##Self(self->ptr_, self->len);                                                      \
        self->len++;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_peek(Self* self, T* out) {                                                          \
        assert(self != NULL);                                                                       \
        if (self->len == 0) {                                                                       \
            return false;                                                                           \
        }                                                                                           \
        if (out != NULL) *out = self->ptr_[0];                                                      \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_pop(Self* self, T* out) {                                                           \
        if (!Self##_peek(self, out)) return false;                                                  \
        self->len--;                                                                                \
        self->ptr_[0] = self->ptr_[self->len];                                                      \
        sift_down_##Self(self->ptr_, self->len, 0);                                                 \
        return true;                                                                                \
    }
#endif
