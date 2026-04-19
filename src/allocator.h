#pragma once
#include "common.h"

typedef u32 allocator_t;

// TODO: Remove this, its annoying. just compute the sizeof myself
#define alloc_alloc(alloc, T, len) alloc_alloc_raw((alloc), sizeof(T) * (len))

#define alloc_realloc(alloc, slice, len)                                                           \
    alloc_realloc_raw((alloc), (slice).ptr, (slice).capacity, sizeof((slice).ptr[0]) * len)

#define alloc_free(alloc, slice)                                                                   \
    alloc_free_raw((alloc), (slice).ptr, sizeof((slice).ptr[0]) * (slice).capacity)

void* alloc_alloc_raw(allocator_t* self, usize size);
void* alloc_realloc_raw(allocator_t* self, void* ptr, usize old_size, usize new_size);
void alloc_free_raw(allocator_t* self, void* ptr, usize size);

typedef enum {
    ARENA_BACKING_FIXED,
    ARENA_BACKING_ALLOC,
    ARENA_BACKING_VIRTUAL,
} arena_backing_kind_t;

typedef struct {
    u8* ptr;
    allocator_t* alloc; // NOTE: Currently unused
    u32 index, capacity;
    arena_backing_kind_t kind;
} arena_t;

#define arena_alloc(arena, T, len) arena_alloc_raw((arena), sizeof(T) * (len))

#define arena_realloc(arena, slice, len)                                                           \
    arena_realloc_raw(                                                                             \
        (arena),                                                                                   \
        (slice).ptr,                                                                               \
        (slice).capacity * sizeof((slice).ptr[0]),                                                 \
        sizeof((slice).ptr[0]) * (len)                                                             \
    )

// TODO: Introduce "checkpoints" and being able to reset the arena to a previous index
arena_t arena_init_fixed(void* buffer, usize size);
arena_t arena_init_alloc(allocator_t* alloc, usize size);
arena_t arena_init_virtual(usize size);
void arena_clear(arena_t* self);
void arena_release(arena_t* self);
void* arena_alloc_raw(arena_t* self, usize size);
void* arena_realloc_raw(arena_t* self, void* ptr, usize old_size, usize new_size);

typedef enum {
    GROW_POLICY_FIXED = 0,
    GROW_POLICY_ARENA,
    GROW_POLICY_ALLOC,
} grow_policy_kind_t;

// TODO: Maybe add here things like load factor or grow strategy
typedef struct {
    union {
        arena_t* arena;
        allocator_t* alloc; // NOTE: Currently unused
    };

    grow_policy_kind_t kind;
} grow_policy_t;
