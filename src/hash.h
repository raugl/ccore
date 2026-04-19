#include "common.h"

// Sets the global seed using OS entropy. This is used when hashing without an explicit seed.
void hash_init(void);
u64 hash_bytes(const void* key, usize size);
u64 hash_bytes_with_seed(const void* key, usize size, u64 seed);
