#pragma once
#include "allocator.h"
#include "common.h"

#define DEQUE_DECL(T) DEQUE_DECL_RAW(T, deque_##T, const T*)

#define DEQUE_DECL_RAW(T, Self, Ptr)                                                                \
    typedef struct Self {                                                                           \
        T*          _data;                                                                          \
        allocator_t _allocator;                                                                     \
        u32         _head, len, capacity;                                                           \
    } Self;                                                                                         \
                                                                                                    \
    Self Self##_init_fixed(T* buffer, usize capacity);                                              \
    Self Self##_init_alloc(allocator_t allocator);                                                  \
    void Self##_destroy(Self* self);                                                                \
                                                                                                    \
    bool Self##_reserve(Self* self, usize capacity);                                                \
    bool Self##_reserve_exact(Self* self, usize capacity);                                          \
    bool Self##_reserve_spare(Self* self, usize count);                                             \
    bool Self##_push_front_many(Self* self, Ptr items, usize count);                                \
    bool Self##_push_back_many(Self* self, Ptr items, usize count);                                 \
    bool Self##_push_front(Self* self, T item);                                                     \
    bool Self##_push_back(Self* self, T item);                                                      \
    bool Self##_pop_front(Self* self, T* out);                                                      \
    bool Self##_pop_back(Self* self, T* out);                                                       \
    bool Self##_front(const Self* self, T* out);                                                    \
    bool Self##_back(const Self* self, T* out);                                                     \
    T* Self##_at(Self* self, usize index);

DEQUE_DECL_RAW(u8, deque_u8, const void*)
DEQUE_DECL(u32)
DEQUE_DECL(u64)
DEQUE_DECL(f32)

#ifdef GENERICS_IMPLEMENTATION
#include "math.h"

// NOTE: This is just a common default, may not be accurate for any specific platform
#define CACHE_LINE_SIZE 64

static void deque_copy_buffer(deque_u8* deque, u8* new_ptr, usize new_capacity, usize size_of) {
    if (deque->_head + deque->len <= deque->capacity) {
        // ..[aaaaaaaaaa].. => [aaaaaaaaaa]....................
        // [aaaaaaaaaaaaaa] => [aaaaaaaaaaaaaa]................
        memcpy(new_ptr, deque->_data + deque->_head, deque->len * size_of);
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
        u32 old__head = deque->_head;
        deque->_head = (u32)new_capacity - right_len;
        memcpy(new_ptr + deque->_head, deque->_data + old__head, right_len * size_of);
        memset_destroyed(deque->_data + old__head, right_len * size_of);
    } else {
        // bbbbbbb]....[aaa => bbbbbbb]....____............[aaa
        // bbbbbbbbb][aaaaa => bbbbbbbbb]______..........[aaaaa
        memcpy(new_ptr + deque->capacity, deque->_data, left_len * size_of);
        memset_destroyed(deque->_data, left_len * size_of);
    }
}

static usize deque_buffer_index(deque_u8* queue, usize index) {
    assert(queue != NULL);
    if (queue->_head + index < queue->capacity) {
        return queue->_head + index;
    } else {
        return queue->_head + index - queue->capacity;
    }
}

#define DEQUE_IMPL(T) DEQUE_IMPL_RAW(T, deque_##T, const T*)

#define DEQUE_IMPL_RAW(T, Self, Ptr)                                                                \
    Self Self##_init_fixed(T* buffer, usize capacity) {                                             \
        assert(buffer != NULL);                                                                     \
        memset_undefined(buffer, capacity * sizeof(T));                                             \
                                                                                                    \
        return (Self) {                                                                             \
            ._data      = buffer,                                                                   \
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
        mem_free(self->_allocator, self->_data, self->capacity);                                    \
        memset_destroyed(self, sizeof(Self));                                                       \
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
        if (mem_resize(self->_allocator, self->_data, self->capacity, new_capacity)) {              \
            deque_copy_buffer((deque_u8*)self, (u8*)self->_data, new_capacity, sizeof(T));          \
            return true;                                                                            \
        }                                                                                           \
                                                                                                    \
        T* new_ptr = mem_alloc(self->_allocator, T, new_capacity);                                  \
        if (new_ptr != NULL) {                                                                      \
            deque_copy_buffer((deque_u8*)self, (u8*)new_ptr, new_capacity, sizeof(T));              \
            mem_free(self->_allocator, self->_data, self->capacity);                                \
            self->capacity = (u32)new_capacity;                                                     \
            self->_data = new_ptr;                                                                  \
            return true;                                                                            \
        }                                                                                           \
        return false;                                                                               \
    }                                                                                               \
                                                                                                    \
    bool Self##_push_front_many(Self* self, Ptr items, usize count) {                               \
        assert(self != NULL);                                                                       \
        if (count == 0) return true;                                                                \
        assert(items != NULL);                                                                      \
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
        if (count == 0) return true;                                                                \
        assert(items != NULL);                                                                      \
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
        if (self->_head == 0) self->_head = self->len;                                              \
        self->_data[--self->_head] = item;                                                          \
        self->len++;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_push_back(Self* self, T item) {                                                     \
        assert(self != NULL);                                                                       \
        if (!Self##_reserve_spare(self, 1)) return false;                                           \
        self->_data[deque_buffer_index((deque_u8*)self, self->len++)] = item;                       \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_pop_front(Self* self, T* out) {                                                     \
        assert(self != NULL);                                                                       \
        if (self->len == 0) return false;                                                           \
                                                                                                    \
        if (out) *out = self->_data[self->_head];                                                   \
        memset_destroyed(self->_data + self->_head, sizeof(T));                                     \
        self->_head = (u32)deque_buffer_index((deque_u8*)self, 1);                                  \
        self->len--;                                                                                \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_pop_back(Self* self, T* out) {                                                      \
        assert(self != NULL);                                                                       \
        if (self->len == 0) return false;                                                           \
                                                                                                    \
        const usize index = deque_buffer_index((deque_u8*)self, --self->len);                       \
        if (out) *out = self->_data[index];                                                         \
        memset_destroyed(self->_data + index, sizeof(T));                                           \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_front(const Self* self, T* out) {                                                   \
        assert(self != NULL);                                                                       \
        assert(out != NULL);                                                                        \
        if (self->len == 0) return false;                                                           \
        *out = self->_data[self->_head];                                                            \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    bool Self##_back(const Self* self, T* out) {                                                    \
        assert(self != NULL);                                                                       \
        assert(out != NULL);                                                                        \
        if (self->len == 0) return false;                                                           \
        *out = self->_data[deque_buffer_index((const deque_u8*)self, self->len - 1)];               \
        return true;                                                                                \
    }                                                                                               \
                                                                                                    \
    T* Self##_at(Self* self, usize index) {                                                         \
        assert(self != NULL);                                                                       \
        if (self->len == 0) return NULL;                                                            \
        return self->_data + deque_buffer_index((deque_u8*)self, index);                            \
    }
#endif
