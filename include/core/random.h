#pragma once
#include "common.h"

static void os_get_random(void* buf, usize size) {
#if defined(__linux__)
#include <sys/random.h>
    getrandom(buf, size, 0);

#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <stdlib.h>
    arc4random_buf(buf, size);

#elif defined(_WIN32)
#include <bcrypt.h>
    BCryptGenRandom(NULL, buf, size, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

#else
    int fd = open("/dev/urandom", O_RDONLY);
    read(fd, buf, size);
    close(fd);
#endif
}

typedef u64 rng_t;

static rng_t rng_seed(u64 seed) {
    return seed;
}

static rng_t rng_seed_entropy(void) {
    rng_t state;
    os_get_random(&state, sizeof(state));
    return state;
}

// Taken from https://github.com/wangyi-fudan/wyhash
static u64 w1rand(rng_t* state) {
    const u64 c = 0xd07ebc63274654c7ull;
    *state += c;
    u128 t = (u128)*state * (*state ^ c);
    return (u64)(t ^ (t >> 64));
}

static u8 rng_u8(rng_t* state) {
    return (u8)(w1rand(state) >> (64 - 8));
}

static u16 rng_u16(rng_t* state) {
    return (u16)(w1rand(state) >> (64 - 16));
}

static u32 rng_u32(rng_t* state) {
    return (u32)(w1rand(state) >> (64 - 32));
}

static u64 rng_u64(rng_t* state) {
    return w1rand(state);
}

// Generates a number in the range [0, 1)
// A float can represent 24 bits of information
static f32 rng_f32(rng_t* state) {
    return (f32)(w1rand(state) >> 40) * 0x1.0p-24f;
}

// Generates a number in the range [0, 1)
// A double can represent 53 bits of information
static f64 rng_f64(rng_t* state) {
    return (f64)(w1rand(state) >> 11) * 0x1.0p-53;
}

static void rng_bytes(rng_t* state, void* out, usize size) {
    usize i = 0;
    for (; i <= size - sizeof(u64); i += sizeof(u64)) {
        u64 x = w1rand(state);
        memcpy((u8*)out + i, &x, sizeof(u64));
    }
    if (i < size) {
        u64 x = w1rand(state);
        memcpy((u8*)out + i, &x, size - i);
    }
}

// Generates a number in the range [start, end)
static u64 rng_range_int(rng_t* state, u64 start, u64 end) {
    u128 mul = w1rand(state) * (end - start);
    return start + (u64)(mul >> 64);
}

// Generates a number in the range [start, end)
static f32 rng_range_f32(rng_t* state, f32 start, f32 end) {
    return start + (end - start) * rng_f32(state);
}

// Generates a number in the range [start, end)
static f64 rng_range_f64(rng_t* state, f64 start, f64 end) {
    return start + (end - start) * rng_f64(state);
}
