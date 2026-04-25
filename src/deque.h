#pragma once
#include "allocator.h"
#include "common.h"

#define DEQUE_DECL(T) DEQUE_DECL_RAW(T, deque_##T)

#define DEQUE_DECL_RAW(T, name)                                                                    \
    typedef struct {                                                                               \
        T* ptr_;                                                                                   \
        grow_policy_t grow_;                                                                       \
        u32 head, len, capacity;                                                                   \
    } name##_t;                                                                                    \
                                                                                                   \
    name##_t name##_init_fixed(T* buffer, usize capacity);                                         \
    name##_t name##_init_arena(arena_t* arena);                                                    \
    name##_t name##_init_alloc(allocator_t* alloc);                                                \
    void name##_release(name##_t* self);                                                           \
                                                                                                   \
    bool name##_ensure_capacity(name##_t* self, usize capacity);                                   \
    bool name##_push_front(name##_t* self, T item);                                                \
    bool name##_push_back(name##_t* self, T item);                                                 \
    bool name##_pop_front(name##_t* self, T* out);                                                 \
    bool name##_pop_back(name##_t* self, T* out);                                                  \
    bool name##_front(name##_t* self, T* out);                                                     \
    bool name##_back(name##_t* self, T* out);

#define deque_at(self, idx)                                                                        \
    (self).ptr_[(validate_idx((idx), (self).len) + (self).head) & ((self).capacity - 1)]

DEQUE_DECL(u8)
DEQUE_DECL(u32)
DEQUE_DECL(u64)
DEQUE_DECL(f32)

#ifdef GENERICS_IMPLEMENTATION
#include "math.h"
#define MIN_CAPACITY 8

#define DEQUE_IMPL(T) DEQUE_IMPL_RAW(T, deque_##T)

#define DEQUE_IMPL_RAW(T, name)                                                                    \
    name##_t name##_init_fixed(T* buffer, usize capacity) {                                        \
        assert(buffer != NULL);                                                                    \
        assert(is_pow2(capacity));                                                                 \
                                                                                                   \
        return (name##_t) {                                                                        \
            .ptr_ = buffer,                                                                        \
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
            arena_try_resize(self->grow_.arena, self->ptr_, self->capacity * sizeof(T), 0);        \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            alloc_free(self->grow_.alloc, self->ptr_, self->capacity * sizeof(T));                 \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            memset(self->ptr_, 0xDE, self->capacity * sizeof(T));                                  \
            break;                                                                                 \
        }                                                                                          \
        memset(self, 0, sizeof(name##_t));                                                         \
    }                                                                                              \
                                                                                                   \
    static void name##_copy_buffer(name##_t* self, T* new_ptr, usize new_capacity) {               \
        if (self->head + self->len <= self->capacity) {                                            \
            /* ..[aaaaaaaaaa].. => [aaaaaaaaaa].................... */                             \
            /* [aaaaaaaaaaaaaa] => [aaaaaaaaaaaaaa]................ */                             \
            memcpy(new_ptr, self->ptr_ + self->head, self->len * sizeof(T));                       \
            return;                                                                                \
        }                                                                                          \
        const u32 right_len = self->capacity - self->head;                                         \
        const u32 left_len = self->len - right_len;                                                \
                                                                                                   \
        if (self->ptr_ != new_ptr) {                                                               \
            /* bb].....[aaaaaaa => [aaaaaa|bb]..................... */                             \
            /* bbbbbbb]....[aaa => [aa|bbbbbbb].................... */                             \
            memcpy(new_ptr, self->ptr_ + self->head, right_len * sizeof(T));                       \
            memcpy(new_ptr + right_len, self->ptr_, left_len * sizeof(T));                         \
            self->head = 0;                                                                        \
        } else if (right_len < left_len) {                                                         \
            /* bb].....[aaaaaaa => ___.....[aaaaaa|bb]............. */                             \
            /* bbbb][aaaaaaaaaa => _____[aaaaaaaaa|bbbb]........... */                             \
            u32 old_head = self->head;                                                             \
            self->head = (u32)new_capacity - right_len;                                            \
            memcpy(new_ptr + self->head, self->ptr_ + old_head, right_len * sizeof(T));            \
        } else {                                                                                   \
            /* bbbbbbb]....[aaa => bbbbbbb]....____............[aaa */                             \
            /* bbbbbbbbb][aaaaa => bbbbbbbbb]______..........[aaaaa */                             \
            memcpy(new_ptr + self->capacity, self->ptr_, left_len * sizeof(T));                    \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    bool name##_ensure_capacity(name##_t* self, usize new_capacity) {                              \
        assert(self != NULL);                                                                      \
        if (self->capacity >= new_capacity) return true;                                           \
        new_capacity = next_pow2_u64(max_usize(MIN_CAPACITY, new_capacity));                       \
        T* new_ptr;                                                                                \
                                                                                                   \
        switch (self->grow_.kind) {                                                                \
        case GROW_POLICY_ARENA: {                                                                  \
            bool grew_in_place = arena_try_resize(                                                 \
                self->grow_.arena,                                                                 \
                self->ptr_,                                                                        \
                self->capacity * sizeof(T),                                                        \
                new_capacity * sizeof(T)                                                           \
            );                                                                                     \
            new_ptr = grew_in_place ? self->ptr_                                                   \
                                    : arena_alloc(self->grow_.arena, new_capacity * sizeof(T));    \
            break;                                                                                 \
        }                                                                                          \
        case GROW_POLICY_ALLOC:                                                                    \
            new_ptr = alloc_alloc(self->grow_.alloc, new_capacity * sizeof(T));                    \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            return false;                                                                          \
        }                                                                                          \
        if (new_ptr == NULL) return false;                                                         \
        name##_copy_buffer(self, new_ptr, new_capacity);                                           \
                                                                                                   \
        if (self->grow_.kind == GROW_POLICY_ALLOC) {                                               \
            alloc_free(self->grow_.alloc, self->ptr_, self->capacity * sizeof(T));                 \
        }                                                                                          \
        self->capacity = (u32)new_capacity;                                                        \
        self->ptr_ = new_ptr;                                                                      \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_push_front(name##_t* self, T item) {                                               \
        assert(self != NULL);                                                                      \
        if (self->len == self->capacity) {                                                         \
            if (!name##_ensure_capacity(self, self->capacity + 1)) return false;                   \
        }                                                                                          \
        const u32 mask = self->capacity - 1;                                                       \
        self->head = (self->head - 1) & mask;                                                      \
        self->ptr_[self->head] = item;                                                             \
        self->len++;                                                                               \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_push_back(name##_t* self, T item) {                                                \
        assert(self != NULL);                                                                      \
        if (self->len == self->capacity) {                                                         \
            if (!name##_ensure_capacity(self, self->capacity + 1)) return false;                   \
        }                                                                                          \
        const u32 mask = self->capacity - 1;                                                       \
        self->ptr_[(self->head + self->len) & mask] = item;                                        \
        self->len++;                                                                               \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_pop_front(name##_t* self, T* out) {                                                \
        assert(self != NULL);                                                                      \
        if (self->len == 0) {                                                                      \
            return false;                                                                          \
        }                                                                                          \
        if (out != NULL) *out = self->ptr_[self->head];                                            \
        const u32 mask = self->capacity - 1;                                                       \
        self->head = (self->head + 1) & mask;                                                      \
        self->len--;                                                                               \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_pop_back(name##_t* self, T* out) {                                                 \
        assert(self != NULL);                                                                      \
        if (self->len == 0) {                                                                      \
            return false;                                                                          \
        }                                                                                          \
        const u32 mask = self->capacity - 1;                                                       \
        self->len--;                                                                               \
        if (out != NULL) *out = self->ptr_[(self->head + self->len) & mask];                       \
        return true;                                                                               \
    }
#endif
