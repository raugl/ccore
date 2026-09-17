#pragma once
#include "common.h"
#include "arena_allocator.h"

typedef struct raw_deque {
    arena_t* _arena;
    u8*      _data;
    u32      _head, len, capacity;
} raw_deque;

#define DEQUE_DECL(T) DEQUE_DECL_RAW(T, deque_##T, const T*)

#define DEQUE_DECL_RAW(T, Self, Ptr)                                                                \
    typedef struct Self {                                                                           \
        arena_t* _arena;                                                                            \
        T*       _data;                                                                             \
        u32      _head, len, capacity;                                                              \
    } Self;                                                                                         \
                                                                                                    \
    Self Self##_init(arena_t* arena);                                                               \
    bool Self##_reserve(Self* self, usize capacity);                                                \
    bool Self##_reserve_exact(Self* self, usize capacity);                                          \
    bool Self##_reserve_spare(Self* self, usize count);                                             \
    bool Self##_push_front_many(Self* self, Ptr items, usize count);                                \
    bool Self##_push_back_many(Self* self, Ptr items, usize count);                                 \
    bool Self##_push_front(Self* self, T item);                                                     \
    bool Self##_push_back(Self* self, T item);                                                      \
    bool Self##_pop_front(Self* self, T* out);                                                      \
    bool Self##_pop_back(Self* self, T* out);                                                       \
    bool Self##_front(Self self, T** out);                                                          \
    bool Self##_back(Self self, T** out);

DEQUE_DECL_RAW(u8, deque_u8, const void*)

#define deque_at(self, index) (self)->_data[_ccore_deque_buffer_index(                              \
        (const void*)((self)),                                                                      \
        _ccore_validate_index((index), (self)->len)                                                 \
    )]

FORCE_INLINE usize _ccore_deque_buffer_index(const raw_deque* deque, usize index) {
    assert(deque != NULL);
    if (deque->_head + index < deque->capacity) {
        return deque->_head + index;
    } else {
        return deque->_head + index - deque->capacity;
    }
}

#ifdef GENERICS_IMPLEMENTATION
#include "math.h"

static void deque_copy_buffer(raw_deque* deque, u8* new_ptr, usize new_capacity, usize size_of) {
    if (deque->_head + deque->len <= deque->capacity) {
        // ..[aaaaaaaaaa].. => ..[aaaaaaaaaa]..................
        // [aaaaaaaaaaaaaa] => [aaaaaaaaaaaaaa]................
        if (deque->_head != new_ptr) {
            memcpy(new_ptr + deque->_head, deque->_data + deque->_head, deque->len * size_of);
        }
        return;
    }
    const u32 right_len = deque->capacity - deque->_head;
    const u32 left_len = deque->len - right_len;

    if (deque->_data != new_ptr) {
        // bb].....[aaaaaaa => [aaaaaa|bb].....................
        // bbbbbbb]....[aaa => [aa|bbbbbbb]....................
        memcpy(new_ptr, deque->_data + deque->_head, right_len * size_of);
        memcpy(new_ptr + right_len, deque->_data, left_len * size_of);
        deque->_head = 0;
    } else if (right_len < left_len) {
        // bb].....[aaaaaaa => ___.....[aaaaaa|bb].............
        // bbbb][aaaaaaaaaa => _____[aaaaaaaaa|bbbb]...........
        u32 old_head = deque->_head;
        deque->_head = (u32)new_capacity - right_len;
        memcpy(new_ptr + deque->_head, deque->_data + old_head, right_len * size_of);
        memset_destroyed(deque->_data + old_head, right_len * size_of);
    } else {
        // bbbbbbb]....[aaa => bbbbbbb]....____............[aaa
        // bbbbbbbbb][aaaaa => bbbbbbbbb]______..........[aaaaa
        memcpy(new_ptr + deque->capacity, deque->_data, left_len * size_of);
        memset_destroyed(deque->_data, left_len * size_of);
    }
}

#define DEQUE_IMPL(T) DEQUE_IMPL_RAW(T, deque_##T, const T*)

#define DEQUE_IMPL_RAW(T, Self, Ptr)                                                                \
    Self Self##_init(arena_t* arena) {                                                              \
        assert(arena != NULL);                                                                      \
        return (Self) { ._arena = arena };                                                          \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve(Self* self, usize new_capacity) {                                           \
        assert(self != NULL);                                                                       \
        if (new_capacity <= self->capacity) return true;                                            \
                                                                                                    \
        new_capacity = (self->capacity == 0) ?                                                      \
            sizeof(cacheline_t) / sizeof(T) :                                                       \
            max_usize(new_capacity, self->capacity + (self->capacity >> 1));                        \
                                                                                                    \
        if (new_capacity > UINT32_MAX) return false;                                                \
        return Self##_reserve_exact(self, new_capacity);                                            \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve_exact(Self* self, usize new_capacity) {                                     \
        assert(self != NULL);                                                                       \
        if (self->capacity >= new_capacity) return true;                                            \
                                                                                                    \
        if (arena_resize(self->_arena, self->_data, self->capacity, new_capacity)) {                \
            deque_copy_buffer((void*)self, (u8*)self->_data, new_capacity, sizeof(T));              \
            return true;                                                                            \
        }                                                                                           \
                                                                                                    \
        arena_replace_info info;                                                                    \
        if (arena_begin_replace(self->_arena, self->_data, self->capacity, new_capacity, &info)) {  \
            deque_copy_buffer((void*)self, info.ptr, new_capacity, sizeof(T));                      \
            arena_commit_replace(self->_arena, info);                                               \
            self->capacity = (u32)new_capacity;                                                     \
            self->_data = info.ptr;                                                                 \
            return true;                                                                            \
        }                                                                                           \
        return false;                                                                               \
    }                                                                                               \
                                                                                                    \
    bool Self##_reserve_spare(Self* self, usize count) {                                            \
        assert(self != NULL);                                                                       \
        return Self##_reserve(self, self->len + count);                                             \
    }                                                                                               \
                                                                                                    \
    bool Self##_push_front_many(Self* self, Ptr items, usize count) {                               \
        assert(self != NULL);                                                                       \
        assert((count == 0) || (items != NULL));                                                    \
                                                                                                    \
        if (count == 0) return true;                                                                \
        if (!Self##_reserve_spare(self, count)) return false;                                       \
                                                                                                    \
        if (self->_head < count) {                                                                  \
            const u32 left_len = (u32)count - self->_head;                                          \
            memcpy(self->_data, (const T*)items + left_len, self->_head * sizeof(T));               \
            self->_head = self->capacity - left_len;                                                \
            memcpy(self->_data + self->_head, items, left_len * sizeof(T));                         \
        } else {                                                                                    \
            self->_head -= (u32)count;                                                              \
            memcpy(self->_data + self->_head, items, count * sizeof(T));                            \
        }                                                                                           \
        self->len += (u32)count;                                                                    \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_push_back_many(Self* self, Ptr items, usize count) {                                \
        assert(self != NULL);                                                                       \
        assert((count == 0) || (items != NULL));                                                    \
                                                                                                    \
        if (count == 0) return true;                                                                \
        if (!Self##_reserve_spare(self, count)) return false;                                       \
                                                                                                    \
        /* FIXME: I dont actually insert anything */                                                \
        self->len += (u32)count;                                                                    \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_push_front(Self* self, T item) {                                                    \
        assert(self != NULL);                                                                       \
        if (!Self##_reserve_spare(self, 1)) return false;                                           \
                                                                                                    \
        if (self->_head == 0) self->_head = self->len;                                              \
        self->_data[--self->_head] = item;                                                          \
        self->len++;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_push_back(Self* self, T item) {                                                     \
        assert(self != NULL);                                                                       \
        if (!Self##_reserve_spare(self, 1)) return false;                                           \
                                                                                                    \
        self->_data[_ccore_deque_buffer_index((void*)self, self->len++)] = item;                    \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_pop_front(Self* self, T* out) {                                                     \
        assert(self != NULL);                                                                       \
        if (self->len == 0) return false;                                                           \
                                                                                                    \
        if (out) *out = self->_data[self->_head];                                                   \
        memset_destroyed(self->_data + self->_head, sizeof(T));                                     \
        self->_head = (u32)_ccore_deque_buffer_index((void*)self, 1);                               \
        self->len--;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_pop_back(Self* self, T* out) {                                                      \
        assert(self != NULL);                                                                       \
        if (self->len == 0) return false;                                                           \
                                                                                                    \
        const usize index = _ccore_deque_buffer_index((void*)self, --self->len);                    \
        if (out) *out = self->_data[index];                                                         \
        memset_destroyed(self->_data + index, sizeof(T));                                           \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_front(Self self, T** out) {                                                         \
        assert(out != NULL);                                                                        \
        if (self.len == 0) return false;                                                            \
        *out = &self._data[self._head];                                                             \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_back(Self self, T** out) {                                                          \
        assert(out != NULL);                                                                        \
        if (self.len == 0) return false;                                                            \
        *out = &self._data[_ccore_deque_buffer_index((void*)&self, self.len - 1)];                  \
        return true;                                                                                \
    }
#endif
