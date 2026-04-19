#pragma once
#include "common.h"

#define MATH_TYPE_DECLS(X)                                                                         \
    X(u8)                                                                                          \
    X(u16)                                                                                         \
    X(u32)                                                                                         \
    X(u64)                                                                                         \
    X(usize)                                                                                       \
    X(i8)                                                                                          \
    X(i16)                                                                                         \
    X(i32)                                                                                         \
    X(i64)                                                                                         \
    X(isize)

#define X(type)                                                                                    \
    static inline type min_##type(type a, type b) {                                                \
        return (a < b) ? a : b;                                                                    \
    }                                                                                              \
    static inline type max_##type(type a, type b) {                                                \
        return (a > b) ? a : b;                                                                    \
    }
MATH_TYPE_DECLS(X);

#define min(type, a, b) min_##type(a, b)
#define max(type, a, b) max_##type(a, b)

static inline bool is_pow2(usize num) {
    return (num & (num - 1)) == 0;
}

static inline bool is_aligned(uintptr_t num) {
    return num % sizeof(max_align_t) == 0;
}

static inline bool is_aligned_ptr(void* ptr) {
    return (uintptr_t)ptr % sizeof(max_align_t) == 0;
}

static inline u32 next_pow2_u32(u32 x) {
    if (x <= 1) return 1;
    return 1ULL << (32 - __builtin_clz(x - 1));
}

static inline u64 next_pow2_u64(u64 x) {
    if (x <= 1) return 1;
    return 1ULL << (64 - __builtin_clzll(x - 1));
}

static inline u32 prev_pow2_u32(u32 x) {
    if (x <= 1) return 0;
    return 1ULL << (32 - __builtin_clz(x - 1));
}

static inline u64 prev_pow2_u64(u64 x) {
    if (x <= 1) return 0;
    return 1ULL << (63 - __builtin_clzll(x - 1));
}

static inline uintptr_t align_forward(uintptr_t address, uintptr_t align) {
    return (address + (align - 1)) & ~(align - 1);
}

static inline void* align_forward_ptr(void* ptr, uintptr_t align) {
    return (void*)(((uintptr_t)ptr + (align - 1)) & ~(align - 1));
}
