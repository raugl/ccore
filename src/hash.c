#include "hash.h"
#include "random.h"

static const u64 rapid_secret[8] = {
    0x2d358dccaa6c78a5ull,
    0x8bb84b93962eacc9ull,
    0x4b33a62ed433d4a3ull,
    0x4d5a2da51de1aa47ull,
    0xa0761d6478bd642full,
    0xe7037ed1a0b428dbull,
    0x90ed1765281c388cull,
    0xaaaaaaaaaaaaaaaaull
};

FORCE_INLINE void mul_mix(u64* a, u64* b) {
    u128 r = *a;
    r *= *b;
#ifdef RAPIDHASH_PROTECTED
    *a ^= (u64)r;
    *b ^= (u64)(r >> 64);
#else
    *a = (u64)r;
    *b = (u64)(r >> 64);
#endif
}

FORCE_INLINE u64 mix(u64 a, u64 b) {
    mul_mix(&a, &b);
    return a ^ b;
}

// NOTE: Assumes little endian
FORCE_INLINE u32 read32(const u8* p) {
    u32 val;
    memcpy(&val, p, sizeof(u32));
    return val;
}

// NOTE: Assumes little endian
FORCE_INLINE u64 read64(const u8* p) {
    u64 val;
    memcpy(&val, p, sizeof(u64));
    return val;
}

static u64 rapidhash(const void* key, usize size, u64 seed, const u64* secret) {
    const u8* p = (const u8*)key;
    seed ^= mix(seed ^ secret[2], secret[1]);
    u64 a = 0, b = 0;
    size_t i = size;
    if (likely(size <= 16)) {
        if (size >= 4) {
            seed ^= size;
            if (size >= 8) {
                const u8* plast = p + size - 8;
                a = read64(p);
                b = read64(plast);
            } else {
                const u8* plast = p + size - 4;
                a = read32(p);
                b = read32(plast);
            }
        } else if (size > 0) {
            a = (((u64)p[0]) << 45) | p[size - 1];
            b = p[size >> 1];
        } else {
            a = 0;
            b = 0;
        }
    } else {
        if (size > 112) {
            u64 see1 = seed, see2 = seed;
            u64 see3 = seed, see4 = seed;
            u64 see5 = seed, see6 = seed;
            do {
                seed = mix(read64(p) ^ secret[0], read64(p + 8) ^ seed);
                see1 = mix(read64(p + 16) ^ secret[1], read64(p + 24) ^ see1);
                see2 = mix(read64(p + 32) ^ secret[2], read64(p + 40) ^ see2);
                see3 = mix(read64(p + 48) ^ secret[3], read64(p + 56) ^ see3);
                see4 = mix(read64(p + 64) ^ secret[4], read64(p + 72) ^ see4);
                see5 = mix(read64(p + 80) ^ secret[5], read64(p + 88) ^ see5);
                see6 = mix(read64(p + 96) ^ secret[6], read64(p + 104) ^ see6);
                p += 112;
                i -= 112;
            } while (i > 112);
            seed ^= see1;
            see2 ^= see3;
            see4 ^= see5;
            seed ^= see6;
            see2 ^= see4;
            seed ^= see2;
        }
        if (i > 16) {
            seed = mix(read64(p) ^ secret[2], read64(p + 8) ^ seed);
            if (i > 32) {
                seed = mix(read64(p + 16) ^ secret[2], read64(p + 24) ^ seed);
                if (i > 48) {
                    seed = mix(read64(p + 32) ^ secret[1], read64(p + 40) ^ seed);
                    if (i > 64) {
                        seed = mix(read64(p + 48) ^ secret[1], read64(p + 56) ^ seed);
                        if (i > 80) {
                            seed = mix(read64(p + 64) ^ secret[2], read64(p + 72) ^ seed);
                            if (i > 96) {
                                seed = mix(read64(p + 80) ^ secret[1], read64(p + 88) ^ seed);
                            }
                        }
                    }
                }
            }
        }
        a = read64(p + i - 16) ^ i;
        b = read64(p + i - 8);
    }
    a ^= secret[1];
    b ^= seed;
    mul_mix(&a, &b);
    return mix(a ^ secret[7], b ^ secret[1] ^ i);
}

static u64 global_seed = 0;

void hash_init(void) {
    os_get_random(&global_seed, sizeof(global_seed));
}

u64 hash_bytes_with_seed(const void* key, usize size, u64 seed) {
    return rapidhash(key, size, seed, rapid_secret);
}

u64 hash_bytes(const void* key, usize size) {
    return hash_bytes_with_seed(key, size, global_seed);
}
