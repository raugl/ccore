#pragma once
#include "allocator.h"
#include "common.h"

#define DEQUE_DECL_RAW(T, name, S)                                                                 \
    typedef struct {                                                                               \
        T* ptr_;                                                                                   \
        grow_policy_t grow_;                                                                       \
        u32 head, len, capacity;                                                                   \
    } name##_t;                                                                                    \
                                                                                                   \
    name##_t name##_init_fixed(T* buffer, usize capacity);                                         \
    name##_t name##_init_arena(arena_t* arena, usize capacity);                                    \
    name##_t name##_init_alloc(allocator_t* alloc, usize capacity);                                \
    void name##_release(name##_t* self);                                                           \
                                                                                                   \
    bool name##_ensure_capacity(name##_t* self, usize capacity);                                   \
    bool name##_push_front(name##_t* self, T item);                                                \
    bool name##_push_back(name##_t* self, T item);                                                 \
    bool name##_pop_front(name##_t* self, T* out);                                                 \
    bool name##_pop_back(name##_t* self, T* out);                                                  \
    bool name##_front(name##_t* self, T* out);                                                     \
    bool name##_back(name##_t* self, T* out);

#define DEQUE_DECL(T) DEQUE_DECL_RAW(T, deque_##T, slice_##T##_t)

#define deque_at(self, idx)                                                                        \
    (self).ptr_[(validate_idx((idx), (self).len) + (self).head) & ((self).capacity - 1)]

DEQUE_DECL(u8);
DEQUE_DECL(u16);
DEQUE_DECL(u32);
DEQUE_DECL(u64);

DEQUE_DECL(i8);
DEQUE_DECL(i16);
DEQUE_DECL(i32);
DEQUE_DECL(i64);

DEQUE_DECL(f32);
DEQUE_DECL(f64);
DEQUE_DECL(bool);

#ifdef GENERICS_IMPLEMENTATION
#include "math.h"
#define MIN_CAPACITY 8

#define DEQUE_IMPL_RAW(T, name)                                                                    \
    name##_t name##_init_fixed(T* buffer, usize capacity) {                                        \
        assert(buffer != NULL);                                                                    \
        assert(is_pow2(capacity));                                                                 \
                                                                                                   \
        return (name##_t) {                                                                        \
            .ptr_ = buffer,                                                                        \
            .capacity = capacity,                                                                  \
            .grow_.kind = GROW_POLICY_FIXED,                                                       \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_arena(arena_t* arena, usize capacity) {                                   \
        assert(arena != NULL);                                                                     \
        if (capacity > 0) {                                                                        \
            capacity = next_pow2_u64(max_usize(MIN_CAPACITY, capacity));                           \
        }                                                                                          \
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
        if (capacity > 0) {                                                                        \
            capacity = next_pow2_u64(max_usize(MIN_CAPACITY, capacity));                           \
        }                                                                                          \
        return (name##_t) {                                                                        \
            .ptr_ = alloc_alloc(alloc, capacity * sizeof(T)),                                      \
            .capacity = capacity,                                                                  \
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
            log_error("attempted to release an arena allocated deque");                            \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            alloc_free(self->grow_.alloc, *self);                                                  \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            log_error("attempted to release a fixed buffer deque");                                \
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
            self->head = new_capacity - right_len;                                                 \
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
        T* new_ptr = self->ptr_;                                                                   \
                                                                                                   \
        switch (self->grow_.kind) {                                                                \
        case GROW_POLICY_ARENA:                                                                    \
            if (!arena_try_extend(self->grow_.arena, *self, new_capacity)) {                       \
                new_ptr = arena_alloc(self->grow_.arena, new_capacity * sizeof(T));                \
                if (new_ptr == NULL) return false;                                                 \
            }                                                                                      \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            new_ptr = alloc_alloc(self->grow_.alloc, new_capacity * sizeof(T));                    \
            if (new_ptr == NULL) return false;                                                     \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            return false; /* NOTE: Fixed buffer cannot grow_ */                                    \
            break;                                                                                 \
        }                                                                                          \
        name##_copy_buffer(self, new_ptr, new_capacity);                                           \
                                                                                                   \
        if (self->grow_.kind == GROW_POLICY_ALLOC) {                                               \
            alloc_free(self->grow_.alloc, *self);                                                  \
        }                                                                                          \
        self->capacity = new_capacity;                                                             \
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
    T name##_pop_front(name##_t* self) {                                                           \
        assert(self != NULL);                                                                      \
        assert(self->len > 0);                                                                     \
        assert(self->ptr_ != NULL);                                                                \
                                                                                                   \
        const u32 mask = self->capacity - 1;                                                       \
        const u32 old_head = self->head;                                                           \
        self->head = (self->head + 1) & mask;                                                      \
        self->len--;                                                                               \
        return self->ptr_[old_head];                                                               \
    }                                                                                              \
                                                                                                   \
    T name##_pop_back(name##_t* self) {                                                            \
        assert(self != NULL);                                                                      \
        assert(self->len > 0);                                                                     \
        assert(self->ptr_ != NULL);                                                                \
                                                                                                   \
        const u32 mask = self->capacity - 1;                                                       \
        return self->ptr_[(self->head + --self->len) & mask];                                      \
    }

#define DEQUE_IMPL(T) DEQUE_IMPL_RAW(T, deque_##T, slice_##T##_t)
#endif
