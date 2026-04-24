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
    static type min_##type(type a, type b) {                                                       \
        return (a < b) ? a : b;                                                                    \
    }                                                                                              \
    static type max_##type(type a, type b) {                                                       \
        return (a > b) ? a : b;                                                                    \
    }
MATH_TYPE_DECLS(X);

static bool is_pow2(usize num) {
    return (num & (num - 1)) == 0;
}

static u16 log2_u32(u32 num) {
    assert(num != 0);
    return 31 - __builtin_clz(num);
}

static u16 log2_u64(u64 num) {
    assert(num != 0);
    return 63 - __builtin_clz(num);
}

static u16 log2_ceil_u32(u32 num) {
    assert(num != 0);
    if (num == 1) return 0;
    return log2_u32(num - 1) + 1;
}

static u16 log2_ceil_u64(u64 num) {
    assert(num != 0);
    if (num == 1) return 0;
    return log2_u64(num - 1) + 1;
}

static bool is_aligned(uintptr_t num) {
    return num % sizeof(max_align_t) == 0;
}

static bool is_aligned_ptr(void* ptr) {
    return (uintptr_t)ptr % sizeof(max_align_t) == 0;
}

static u32 next_pow2_u32(u32 num) {
    if (num <= 1) return 1;
    return 1ULL << (32 - __builtin_clz(num - 1));
}

static u64 next_pow2_u64(u64 num) {
    if (num <= 1) return 1;
    return 1ULL << (64 - __builtin_clzll(num - 1));
}

static u32 prev_pow2_u32(u32 num) {
    if (num <= 1) return 0;
    return 1ULL << (31 - __builtin_clz(num - 1));
}

static u64 prev_pow2_u64(u64 num) {
    if (num <= 1) return 0;
    return 1ULL << (63 - __builtin_clzll(num - 1));
}

static uintptr_t align_forward(uintptr_t address, uintptr_t align) {
    return (address + (align - 1)) & ~(align - 1);
}

static void* align_forward_ptr(void* ptr, uintptr_t align) {
    return (void*)(((uintptr_t)ptr + (align - 1)) & ~(align - 1));
}
