#pragma once
#include "common.h"

#define MATH_TYPE_DECLS(X)                                                                          \
    X(u8)                                                                                           \
    X(u16)                                                                                          \
    X(u32)                                                                                          \
    X(u64)                                                                                          \
    X(usize)                                                                                        \
    X(i8)                                                                                           \
    X(i16)                                                                                          \
    X(i32)                                                                                          \
    X(i64)                                                                                          \
    X(isize)                                                                                        \
    X(f32)                                                                                          \
    X(f64)                                                                                          \
    X(f80)

#define X(type)                                                                                     \
    FORCE_INLINE type min_##type(type a, type b) {                                                  \
        return (a < b) ? a : b;                                                                     \
    }                                                                                               \
    FORCE_INLINE type max_##type(type a, type b) {                                                  \
        return (a > b) ? a : b;                                                                     \
    }                                                                                               \
    FORCE_INLINE type clamp_##type(type num, type min, type max) {                                  \
        return max_##type(min, min_##type(max, num));                                               \
    }

MATH_TYPE_DECLS(X)

FORCE_INLINE bool is_pow2(u64 num) {
    return (num & (num - 1)) == 0;
}

FORCE_INLINE u16 log2_u32(u32 num) {
    assert(num != 0);
    return (u16)(31 - __builtin_clz(num));
}

FORCE_INLINE u16 log2_u64(u64 num) {
    assert(num != 0);
    return (u16)(63 - __builtin_clzll(num));
}

FORCE_INLINE u16 log2_ceil_u32(u32 num) {
    assert(num != 0);
    if (num == 1) return 0;
    return log2_u32(num - 1) + 1;
}

FORCE_INLINE u16 log2_ceil_u64(u64 num) {
    assert(num != 0);
    if (num == 1) return 0;
    return log2_u64(num - 1) + 1;
}

FORCE_INLINE bool is_aligned(uintptr_t num, u64 align) {
    assert(is_pow2(align));
    return (num & (align - 1)) == 0;
}

FORCE_INLINE bool is_aligned_ptr(void* ptr, u64 align) {
    assert(is_pow2(align));
    return ((uintptr_t)ptr & (align - 1)) == 0;
}

FORCE_INLINE u32 next_pow2_u32(u32 num) {
    if (num <= 1) return 1;
    return 1u << (32 - __builtin_clz(num - 1));
}

FORCE_INLINE u64 next_pow2_u64(u64 num) {
    if (num <= 1) return 1;
    return 1ull << (64 - __builtin_clzll(num - 1));
}

FORCE_INLINE u32 prev_pow2_u32(u32 num) {
    if (num <= 1) return 0;
    return 1u << (31 - __builtin_clz(num - 1));
}

FORCE_INLINE u64 prev_pow2_u64(u64 num) {
    if (num <= 1) return 0;
    return 1ull << (63 - __builtin_clzll(num - 1));
}

FORCE_INLINE uintptr_t align_forward(uintptr_t address, uintptr_t align) {
    return (address + (align - 1)) & ~(align - 1);
}

FORCE_INLINE void* align_forward_ptr(void* ptr, uintptr_t align) {
    return (void*)(((uintptr_t)ptr + (align - 1)) & ~(align - 1));
}
