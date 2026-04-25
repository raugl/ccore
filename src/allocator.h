#pragma once
#include "common.h"
#include "math.h"

typedef u32 allocator_t;

// TODO: Add poisioning in debug builds when releasing buffers, and zeroing out when allocating.
void* alloc_alloc(allocator_t* self, usize size);
void* alloc_realloc(allocator_t* self, void* ptr, usize old_size, usize new_size);
void alloc_free(allocator_t* self, void* ptr, usize size);

typedef enum {
    ARENA_BACKING_FIXED,
    ARENA_BACKING_ALLOC,
    ARENA_BACKING_VIRTUAL,
} arena_backing_kind_t;

typedef struct {
    u8* ptr_;
    allocator_t* alloc; // NOTE: Currently unused
    u32 index, capacity;
    arena_backing_kind_t kind;
} arena_t;

// TODO: Introduce "checkpoints" and being able to reset the arena to a previous index
arena_t arena_init_fixed(void* buffer, usize size);
arena_t arena_init_alloc(allocator_t* alloc, usize size);
arena_t arena_init_virtual(usize size);
void arena_clear(arena_t* self);
void arena_release(arena_t* self);
void* arena_alloc(arena_t* self, usize size);
void* arena_realloc(arena_t* self, void* ptr, usize old_size, usize new_size);
bool arena_try_resize(arena_t* self, void* ptr, usize old_size, usize new_size);

typedef enum {
    GROW_POLICY_FIXED = 0,
    GROW_POLICY_ARENA,
    GROW_POLICY_ALLOC,
} grow_policy_kind_t;

typedef struct {
    union {
        arena_t* arena;
        allocator_t* alloc; // NOTE: Currently unused
    };

    grow_policy_kind_t kind;
} grow_policy_t;

static void memswap(void* a, void* b, usize n) {
    u8 tmp[256];
    u8* ptr_a = a;
    u8* ptr_b = b;

    while (n > 0) {
        usize chunk = min_usize(n, sizeof(tmp));
        memcpy(tmp, ptr_a, chunk);
        memcpy(ptr_a, ptr_b, chunk);
        memcpy(ptr_b, tmp, chunk);

        ptr_a += chunk;
        ptr_b += chunk;
        n -= chunk;
    }
}
