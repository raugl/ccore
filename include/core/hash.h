#pragma once
#include "common.h"

// Sets the global seed using OS entropy. This is used when hashing without an explicit seed.
void hash_init(void);
u64 hash_bytes(const void* key, usize size);
u64 hash_bytes_with_seed(const void* key, usize size, u64 seed);

static u64 hash_mix(u64 a, u64 b) {
    u64 x = a ^ (b + 0x9e3779b97f4a7c15);
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9;
    x ^= x >> 27;
    x *= 0x94d049bb133111eb;
    x ^= x >> 31;
    return x;
}

static u64 hash_ptr(const void* key, usize size) {
    (void)size;
    void* ptr = *(void* const*)key;
    return hash_mix((u64)ptr, 0xC0FFEE);
}
