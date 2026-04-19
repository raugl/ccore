#pragma once
#include "allocator.h"
#include "common.h"

#define DEQUE_RAW_DECL(T, name, S)                                                                 \
    typedef struct {                                                                               \
        T* ptr;                                                                                    \
        grow_policy_t grow;                                                                        \
        u32 head, len, capacity;                                                                   \
    } name##_t;                                                                                    \
                                                                                                   \
    name##_t name##_init_fixed(T* buffer, usize capacity);                                         \
    name##_t name##_init_arena(arena_t* arena, usize capacity);                                    \
    name##_t name##_init_alloc(allocator_t* alloc, usize capacity);                                \
    void name##_release(name##_t* self);                                                           \
    bool name##_ensure_capacity(name##_t* self, usize capacity);                                   \
    bool name##_push_front(name##_t* self, T data);                                                \
    bool name##_push_back(name##_t* self, T data);                                                 \
    T name##_pop_front(name##_t* self);                                                            \
    T name##_pop_back(name##_t* self);

#define DEQUE_DECL(T) DEQUE_RAW_DECL(T, deque_##T, slice_##T##_t)

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

#define DEQUE_RAW_IMPL(T, name)                                                                    \
    name##_t name##_init_fixed(T* buffer, usize capacity) {                                        \
        assert(buffer != NULL);                                                                    \
        assert(is_pow2(capacity));                                                                 \
        assert(is_aligned_ptr(buffer));                                                            \
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
        if (capacity > 0) {                                                                        \
            capacity = next_pow2_u64(max_u32(MIN_CAPACITY, new_capacity));                         \
        }                                                                                          \
        return (name##_t) {                                                                        \
            .ptr = arena_alloc_raw(arena, capacity * sizeof(T)),                                   \
            .capacity = capacity,                                                                  \
            .grow.arena = arena,                                                                   \
            .grow.kind = GROW_POLICY_ARENA,                                                        \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    name##_t name##_init_alloc(allocator_t* alloc, usize capacity) {                               \
        assert(alloc != NULL);                                                                     \
        if (capacity > 0) {                                                                        \
            capacity = next_pow2_u64(max_u32(MIN_CAPACITY, new_capacity));                         \
        }                                                                                          \
        return (name##_t) {                                                                        \
            .ptr = alloc_alloc_raw(alloc, capacity * sizeof(T)),                                   \
            .capacity = capacity,                                                                  \
            .grow.alloc = alloc,                                                                   \
            .grow.kind = GROW_POLICY_ALLOC,                                                        \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    void name##_release(name##_t* self) {                                                          \
        assert(self != NULL);                                                                      \
                                                                                                   \
        switch (self->grow.kind) {                                                                 \
        case GROW_POLICY_ARENA:                                                                    \
            log_error("attempted to release an arena allocated deque");                            \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            alloc_free(self->grow.alloc, *self);                                                   \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            log_error("attempted to release a fixed buffer deque");                                \
            break;                                                                                 \
        }                                                                                          \
        memset(self, 0, sizeof(name##_t));                                                         \
    }                                                                                              \
                                                                                                   \
    bool name##_ensure_capacity(name##_t* self, usize new_capacity) {                              \
        assert(self != NULL);                                                                      \
        assert(new_capacity > self->capacity);                                                     \
                                                                                                   \
        new_capacity = next_pow2_u64(max_u32(MIN_CAPACITY, new_capacity));                         \
        const bool wrapped = self->head + self->len > self->capacity;                              \
        T* new_ptr = NULL;                                                                         \
                                                                                                   \
        switch (self->grow.kind) {                                                                 \
        case GROW_POLICY_ARENA:                                                                    \
            new_ptr = arena_realloc(self->grow.arena, *self, new_capacity);                        \
            if (new_ptr == NULL) {                                                                 \
                new_ptr = arena_alloc_raw(self->grow.arena, new_capacity * sizeof(T));             \
                if (new_ptr == NULL) return false;                                                 \
            }                                                                                      \
            break;                                                                                 \
        case GROW_POLICY_ALLOC:                                                                    \
            new_ptr = alloc_alloc_raw(self->grow.alloc, new_capacity * sizeof(T));                 \
            if (new_ptr == NULL) return false;                                                     \
            break;                                                                                 \
        case GROW_POLICY_FIXED:                                                                    \
            panic("fixed buffer deque cannot allocate new buffer");                                \
            break;                                                                                 \
        }                                                                                          \
                                                                                                   \
        if (wrapped) {                                                                             \
            const u32 right_len = self->capacity - self->head;                                     \
            const u32 left_len = self->len - right_len;                                            \
                                                                                                   \
            if (self->ptr != new_ptr) {                                                            \
                /* bb].....[aaaaaaa => [aaaaaa|bb]..................... */                         \
                /* bbbbbbb]....[aaa => [aa|bbbbbbb].................... */                         \
                memcpy(new_ptr, self->ptr + self->head, right_len * sizeof(T));                    \
                memcpy(new_ptr + right_len, self->ptr, left_len * sizeof(T));                      \
                self->head = 0;                                                                    \
            } else if (right_len < left_len) {                                                     \
                /* bb].....[aaaaaaa => ___.....[aaaaaa|bb]............. */                         \
                /* bbbb][aaaaaaaaaa => _____[aaaaaaaaa|bbbb]........... */                         \
                u32 old_head = self->head;                                                         \
                self->head = new_capacity - right_len;                                             \
                memcpy(new_ptr + self->head, self->ptr + old_head, right_len * sizeof(T));         \
            } else {                                                                               \
                /* bbbbbbb]....[aaa => bbbbbbb]....____............[aaa */                         \
                /* bbbbbbbbb][aaaaa => bbbbbbbbb]______..........[aaaaa */                         \
                memcpy(new_ptr + self->capacity, self->ptr, left_len * sizeof(T));                 \
            }                                                                                      \
        } else if (self->ptr != new_ptr) {                                                         \
            /* ..[aaaaaaaaaa].. => [aaaaaaaaaa].................... */                             \
            /* [aaaaaaaaaaaaaa] => [aaaaaaaaaaaaaa]................ */                             \
            memcpy(new_ptr, self->ptr + self->head, self->len * sizeof(T));                        \
        }                                                                                          \
                                                                                                   \
        if (self->grow.kind == GROW_POLICY_ALLOC) {                                                \
            alloc_free(self->grow.alloc, *self);                                                   \
        }                                                                                          \
        self->capacity = new_capacity;                                                             \
        self->ptr = new_ptr;                                                                       \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_push_front(name##_t* self, T data) {                                               \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->len == self->capacity) {                                                         \
            if (!name##_ensure_capacity(self, self->capacity + 1)) return false;                   \
        }                                                                                          \
        const u32 mask = self->capacity - 1;                                                       \
        self->head = (self->head - 1) & mask;                                                      \
        self->ptr[self->head] = data;                                                              \
        self->len++;                                                                               \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    bool name##_push_back(name##_t* self, T data) {                                                \
        assert(self != NULL);                                                                      \
                                                                                                   \
        if (self->len == self->capacity) {                                                         \
            if (!name##_ensure_capacity(self, self->capacity + 1)) return false;                   \
        }                                                                                          \
        const u32 mask = self->capacity - 1;                                                       \
        self->ptr[(self->head + self->len) & mask] = data;                                         \
        self->len++;                                                                               \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    T name##_pop_front(name##_t* self) {                                                           \
        assert(self != NULL);                                                                      \
        assert(self->len > 0);                                                                     \
        assert(self->ptr != NULL);                                                                 \
                                                                                                   \
        const u32 mask = self->capacity - 1;                                                       \
        const u32 old_head = self->head;                                                           \
        self->head = (self->head + 1) & mask;                                                      \
        self->len--;                                                                               \
        return self->ptr[old_head];                                                                \
    }                                                                                              \
                                                                                                   \
    T name##_pop_back(name##_t* self) {                                                            \
        assert(self != NULL);                                                                      \
        assert(self->len > 0);                                                                     \
        assert(self->ptr != NULL);                                                                 \
                                                                                                   \
        const u32 mask = self->capacity - 1;                                                       \
        return self->ptr[(self->head + --self->len) & mask];                                       \
    }

#define DEQUE_IMPL(T) DEQUE_RAW_IMPL(T, deque_##T, slice_##T##_t)
#endif
