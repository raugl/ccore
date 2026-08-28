#pragma once
#include "common.h"

#define SLICE_DECL(T) SLICE_DECL_RAW(T, slice_##T)

#define SLICE_DECL_RAW(T, Self)                                                                     \
    typedef struct {                                                                                \
        T*    ptr;                                                                                  \
        usize len;                                                                                  \
    } Self;                                                                                         \
                                                                                                    \
    Self Self##_slice(Self self, usize idx, usize len);                                             \
    bool Self##_equal(Self a, Self b);                                                              \

SLICE_DECL(u8)
SLICE_DECL(u32)
SLICE_DECL(u64)
SLICE_DECL(f32)
SLICE_DECL_RAW(const char, string)
SLICE_DECL_RAW(void, slice_raw)

#define cstr(str)                                                                                   \
    (string) {                                                                                      \
        .ptr = (str), .len = strlen((str))                                                          \
    }

bool string_starts_with(string str, string prefix);
bool string_starts_with_cstr(string str, cstring prefix);
bool cstring_starts_with(cstring str, cstring prefix);
bool cstring_starts_with_str(cstring str, string prefix);

#ifdef GENERICS_IMPLEMENTATION

#define SLICE_IMPL(T) SLICE_IMPL_RAW(T, slice_##T)

#define SLICE_IMPL_RAW(T, Self)                                                                     \
    Self Self##_slice(Self self, usize idx, usize len) {                                            \
        assert(idx < self.len);                                                                     \
        assert(idx + len <= self.len);                                                              \
                                                                                                    \
        return (Self) {                                                                             \
            .ptr = self.ptr + idx,                                                                  \
            .len = (len > 0) ? len : self.len,                                                      \
        };                                                                                          \
    }                                                                                               \
                                                                                                    \
    bool Self##_equal(Self a, Self b) {                                                             \
        if (a.len != b.len) return false;                                                           \
                                                                                                    \
        for (usize i = 0; i < a.len; ++i) {                                                         \
            /* TODO: if (!equal_fn(a.ptr[i], b.ptr[i])) return false; */                            \
            if (a.ptr[i] != b.ptr[i]) return false;                                                 \
        }                                                                                           \
        return true;                                                                                \
    }
#endif
