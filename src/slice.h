#pragma once
#include "common.h"

#define SLICE_DECL_RAW(T, name)                                                                    \
    typedef struct {                                                                               \
        T* ptr;                                                                                    \
        usize len;                                                                                 \
    } name##_t;                                                                                    \
                                                                                                   \
    name##_t name##_slice(name##_t self, usize idx, usize len);

#define SLICE_DECL(T) SLICE_DECL_RAW(T, slice_##T)

SLICE_DECL(u8)
SLICE_DECL(u32)
SLICE_DECL(u64)
SLICE_DECL(f32)
SLICE_DECL_RAW(const char, string)
SLICE_DECL_RAW(void, slice_raw)

#define cstr(string)                                                                               \
    (string_t) {                                                                                   \
        .ptr = (string), .len = strlen((string))                                                   \
    }

#ifdef GENERICS_IMPLEMENTATION

#define SLICE_IMPL_RAW(T, name)                                                                    \
    name##_t name##_slice(name##_t self, usize idx, usize len) {                                   \
        assert(idx < self.len);                                                                    \
        assert(idx + len <= self.len);                                                             \
                                                                                                   \
        return (name##_t) {                                                                        \
            .ptr = self.ptr + idx,                                                                 \
            .len = (len > 0) ? len : self.len,                                                     \
        };                                                                                         \
    }

#define SLICE_IMPL(T) SLICE_IMPL_RAW(T, slice_##T)
#endif
