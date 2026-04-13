#pragma once
#include "common.h"

typedef void allocator_t;

// FIXME: Should assert on failure
#define alloc_alloc(alloc, value_t, len) malloc(sizeof(value_t) * (len))

// FIXME: Should assert on failure
#define alloc_realloc(alloc, slice, len) realloc((slice).ptr, sizeof((slice).ptr[0]) * (len))

#define alloc_free(alloc, slice) free((slice).ptr)

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

#define arena_alloc(arena, value_t, size) arena_alloc_impl((arena), sizeof(value_t) * (size))

#define arena_realloc(arena, slice, size)                                                          \
    arena_realloc_impl(                                                                            \
        (arena),                                                                                   \
        (slice).ptr,                                                                               \
        (slice).len * sizeof((slice).ptr[0]),                                                      \
        sizeof((slice).ptr[0]) * (size)                                                            \
    )

// TODO: Introduce "checkpoints" and being able to reset the arena to a previous index
arena_t arena_init_fixed(void* buffer, u32 size);
arena_t arena_init_alloc(allocator_t* alloc, u32 size);
arena_t arena_init_virtual(u32 size);
void arena_clear(arena_t* self);
void arena_release(arena_t* self);
void* arena_alloc_impl(arena_t* self, u32 size);
void* arena_realloc_impl(arena_t* self, void* ptr, u32 old_size, u32 new_size);

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
