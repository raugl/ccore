#pragma once
#include "allocator.h"
#include "common.h"

#define DEQUE_DECL(T) DEQUE_DECL_RAW(T, deque_##T, const T*)

#define DEQUE_DECL_RAW(T, Self, Ptr)                                                                \
    typedef struct {                                                                                \
        T* _ptr;                                                                                    \
        allocator_t allocator;                                                                      \
        u32 _head, len, capacity;                                                                   \
    } Self;                                                                                         \
                                                                                                    \
    Self Self##_init_fixed(T* buffer, usize capacity);                                              \
    Self Self##_init_alloc(allocator_t allocator);                                                  \
    void Self##_release(Self* self);                                                                \
                                                                                                    \
    bool Self##_ensure_capacity(Self* self, usize capacity);                                        \
    bool Self##_push_front_many(Self* self, Ptr items, usize len);                                  \
    bool Self##_push_back_many(Self* self, Ptr items, usize len);                                   \
    bool Self##_push_front(Self* self, T item);                                                     \
    bool Self##_push_back(Self* self, T item);                                                      \
    bool Self##_pop_front(Self* self, T* out);                                                      \
    bool Self##_pop_back(Self* self, T* out);                                                       \
    bool Self##_front(Self* self, T* out);                                                          \
    bool Self##_back(Self* self, T* out);

#define deque_at(self, idx)                                                                         \
    (self)._ptr[(validate_idx((idx), (self).len) + (self)._head) & ((self).capacity - 1)]

DEQUE_DECL_RAW(u8, deque_u8, const void*)
DEQUE_DECL(u32)
DEQUE_DECL(u64)
DEQUE_DECL(f32)

#ifdef GENERICS_IMPLEMENTATION
#include "math.h"
#define MIN_CAPACITY 8

#define DEQUE_IMPL(T) DEQUE_IMPL_RAW(T, deque_##T, const T*)

#define DEQUE_IMPL_RAW(T, Self, Ptr)                                                                \
    Self Self##_init_fixed(T* buffer, usize capacity) {                                             \
        assert(buffer != NULL);                                                                     \
        assert(is_pow2(capacity));                                                                  \
                                                                                                    \
        return (Self) {                                                                             \
            ._ptr = buffer,                                                                         \
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
    void Self##_release(Self* self) {                                                               \
        assert(self != NULL);                                                                       \
        mem_free(self->allocator, self->_ptr, self->capacity);                                      \
        memset(self, 0, sizeof(Self));                                                              \
    }                                                                                               \
                                                                                                    \
    static void Self##_copy_buffer(Self* self, T* new_ptr, usize new_capacity) {                    \
        if (self->_head + self->len <= self->capacity) {                                            \
            /* ..[aaaaaaaaaa].. => [aaaaaaaaaa].................... */                              \
            /* [aaaaaaaaaaaaaa] => [aaaaaaaaaaaaaa]................ */                              \
            memcpy(new_ptr, self->_ptr + self->_head, self->len * sizeof(T));                       \
            return;                                                                                 \
        }                                                                                           \
        const u32 right_len = self->capacity - self->_head;                                         \
        const u32 left_len = self->len - right_len;                                                 \
                                                                                                    \
        if (self->_ptr != new_ptr) {                                                                \
            /* bb].....[aaaaaaa => [aaaaaa|bb]..................... */                              \
            /* bbbbbbb]....[aaa => [aa|bbbbbbb].................... */                              \
            memcpy(new_ptr, self->_ptr + self->_head, right_len * sizeof(T));                       \
            memcpy(new_ptr + right_len, self->_ptr, left_len * sizeof(T));                          \
            self->_head = 0;                                                                        \
        } else if (right_len < left_len) {                                                          \
            /* bb].....[aaaaaaa => ___.....[aaaaaa|bb]............. */                              \
            /* bbbb][aaaaaaaaaa => _____[aaaaaaaaa|bbbb]........... */                              \
            u32 old__head = self->_head;                                                            \
            self->_head = (u32)new_capacity - right_len;                                            \
            memcpy(new_ptr + self->_head, self->_ptr + old__head, right_len * sizeof(T));           \
        } else {                                                                                    \
            /* bbbbbbb]....[aaa => bbbbbbb]....____............[aaa */                              \
            /* bbbbbbbbb][aaaaa => bbbbbbbbb]______..........[aaaaa */                              \
            memcpy(new_ptr + self->capacity, self->_ptr, left_len * sizeof(T));                     \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    bool Self##_ensure_capacity(Self* self, usize new_capacity) {                                   \
        assert(self != NULL);                                                                       \
        if (self->capacity >= new_capacity) return true;                                            \
        new_capacity = next_pow2_u64(max_usize(MIN_CAPACITY, new_capacity));                        \
                                                                                                    \
        if (mem_resize(self->allocator, self->_ptr, self->capacity, new_capacity)) {                \
            Self##_copy_buffer(self, self->_ptr, new_capacity);                                     \
            return true;                                                                            \
        }                                                                                           \
        T* new_ptr = mem_alloc(self->allocator, T, new_capacity);                                   \
        if (new_ptr != NULL) {                                                                      \
            Self##_copy_buffer(self, new_ptr, new_capacity);                                        \
            mem_free(self->allocator, self->_ptr, self->capacity);                                  \
            self->capacity = (u32)new_capacity;                                                     \
            self->_ptr = new_ptr;                                                                   \
            return true;                                                                            \
        }                                                                                           \
        return false;                                                                               \
    }                                                                                               \
                                                                                                    \
    bool Self##_push_front_many(Self* self, Ptr items, usize len) {                                 \
        assert(self != NULL);                                                                       \
        if (len == 0) return true;                                                                  \
        assert(items != NULL);                                                                      \
                                                                                                    \
        if (self->len + len > self->capacity) {                                                     \
            if (!Self##_ensure_capacity(self, self->capacity + len)) return false;                  \
        }                                                                                           \
        if (self->_head < len) {                                                                    \
            memcpy(self->_ptr, (const T*)items + len - self->_head, self->_head * sizeof(T));       \
            self->_head = self->capacity - (u32)len + self->_head;                                  \
            memcpy(                                                                                 \
                self->_ptr + self->_head,                                                           \
                (const T*)items,                                                                    \
                (self->capacity - self->_head) * sizeof(T)                                          \
            );                                                                                      \
        } else {                                                                                    \
            self->_head -= (u32)len;                                                                \
            memcpy(self->_ptr + self->_head, items, len * sizeof(T));                               \
        }                                                                                           \
        self->len += (u32)len;                                                                      \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_push_back_many(Self* self, Ptr items, usize len) {                                  \
        assert(self != NULL);                                                                       \
        if (len == 0) return true;                                                                  \
        assert(items != NULL);                                                                      \
                                                                                                    \
        if (self->len + len > self->capacity) {                                                     \
            if (!Self##_ensure_capacity(self, self->capacity + len)) return false;                  \
        }                                                                                           \
        self->len += (u32)len;                                                                      \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_push_front(Self* self, T item) {                                                    \
        assert(self != NULL);                                                                       \
        if (self->len == self->capacity) {                                                          \
            if (!Self##_ensure_capacity(self, self->capacity + 1)) return false;                    \
        }                                                                                           \
        const u32 mask = self->capacity - 1;                                                        \
        self->_head = (self->_head - 1) & mask;                                                     \
        self->_ptr[self->_head] = item;                                                             \
        self->len++;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_push_back(Self* self, T item) {                                                     \
        assert(self != NULL);                                                                       \
        if (self->len == self->capacity) {                                                          \
            if (!Self##_ensure_capacity(self, self->capacity + 1)) return false;                    \
        }                                                                                           \
        const u32 mask = self->capacity - 1;                                                        \
        self->_ptr[(self->_head + self->len) & mask] = item;                                        \
        self->len++;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_pop_front(Self* self, T* out) {                                                     \
        assert(self != NULL);                                                                       \
        if (self->len == 0) {                                                                       \
            return false;                                                                           \
        }                                                                                           \
        if (out != NULL) *out = self->_ptr[self->_head];                                            \
        const u32 mask = self->capacity - 1;                                                        \
        self->_head = (self->_head + 1) & mask;                                                     \
        self->len--;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_pop_back(Self* self, T* out) {                                                      \
        assert(self != NULL);                                                                       \
        if (self->len == 0) {                                                                       \
            return false;                                                                           \
        }                                                                                           \
        const u32 mask = self->capacity - 1;                                                        \
        self->len--;                                                                                \
        if (out != NULL) *out = self->_ptr[(self->_head + self->len) & mask];                       \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_front(Self* self, T* out) {                                                         \
        assert(self != NULL);                                                                       \
        if (self->len == 0) {                                                                       \
            return false;                                                                           \
        }                                                                                           \
        if (out != NULL) *out = self->_ptr[self->_head];                                            \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_back(Self* self, T* out) {                                                          \
        assert(self != NULL);                                                                       \
        if (self->len == 0) {                                                                       \
            return false;                                                                           \
        }                                                                                           \
        const u32 mask = self->capacity - 1;                                                        \
        if (out != NULL) *out = self->_ptr[(self->_head + self->len - 1) & mask];                   \
        return true;                                                                                \
    }
#endif
