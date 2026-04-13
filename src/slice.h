#pragma once
#include "common.h"

#define SLICE_DECL(T, name)                                                                        \
    typedef struct {                                                                               \
        T* ptr;                                                                                    \
        u32 len;                                                                                   \
    } slice_##name##_t;                                                                            \
                                                                                                   \
    slice_##name##_t slice_##name##_slice(slice_##name##_t self, u32 idx, u32 len);

SLICE_DECL(u8, u8);
SLICE_DECL(u16, u16);
SLICE_DECL(u32, u32);
SLICE_DECL(u64, u64);

SLICE_DECL(i8, i8);
SLICE_DECL(i16, i16);
SLICE_DECL(i32, i32);
SLICE_DECL(i64, i64);

SLICE_DECL(f32, f32);
SLICE_DECL(f64, f64);
SLICE_DECL(bool, bool);

#ifdef GENERICS_IMPLEMENTATION

#define SLICE_IMPL(T, name)                                                                        \
    slice_##name##_t slice_##name##_slice(slice_##name##_t self, u32 idx, u32 len) {               \
        assert(idx < self.len);                                                                    \
        assert(idx + len <= self.len);                                                             \
                                                                                                   \
        return (slice_##name##_t) {                                                                \
            .ptr = self.ptr + idx,                                                                 \
            .len = (len > 0) ? len : self.len,                                                     \
        };                                                                                         \
    }
#endif
