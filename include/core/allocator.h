#pragma once
#include "common.h"
#include "math.h"

typedef struct {
    void* (*alloc  )(void* ctx, usize size, usize align);
    bool  (*resize )(void* ctx, void* ptr, usize old_size, usize new_size);
    void* (*realloc)(void* ctx, void* ptr, usize old_size, usize new_size, usize align);
    void  (*free   )(void* ctx, void* ptr, usize size);
} allocator_vtable_t;

typedef struct {
    void* ctx;
    const allocator_vtable_t* vtable;
} allocator_t;

#define mem_alloc_aligned(allocator, T, len, align)                                                 \
    (allocator).vtable->alloc((allocator).ctx, (len) * sizeof(T), align)

#define mem_alloc(allocator, T, len) mem_alloc_aligned(allocator, T, len, _Alignof(T))

#define mem_resize(allocator, ptr, old_len, new_len)                                                \
    (allocator).vtable->resize(                                                                     \
        (allocator).ctx,                                                                            \
        (ptr),                                                                                      \
        (old_len) * sizeof(*(ptr)),                                                                 \
        (new_len) * sizeof(*(ptr))                                                                  \
    )

#define mem_realloc_aligned(allocator, ptr, old_len, new_len, align)                                \
    (allocator).vtable->realloc(                                                                    \
        (allocator).ctx,                                                                            \
        (ptr),                                                                                      \
        (old_len) * sizeof(*(ptr)),                                                                 \
        (new_len) * sizeof(*(ptr)),                                                                 \
        align                                                                                       \
    )

#define mem_realloc(allocator, ptr, old_len, new_len)                                               \
    mem_realloc_aligned(allocator, ptr, old_len, new_len, _Alignof(*(ptr)))

#define mem_free(allocator, ptr, len)                                                               \
    (allocator).vtable->free((allocator).ctx, (ptr), (len) * sizeof(*(ptr)))

typedef struct {
    u8* ptr;
    allocator_t allocator;
    u32 offset, capacity;
} arena_t;

#define arena_alloc(arena, T, len) arena_alloc_aligned(arena, T, len, _Alignof(T))

#define arena_alloc_aligned(arena, T, len, align) arena_alloc_raw((arena), len * sizeof(T), align)

#define arena_resize(arena, ptr, old_len, new_len)                                                  \
    arena_resize_raw((arena), (ptr), (old_len) * sizeof(*(ptr)), (new_len) * sizeof(*(ptr)))

#define arena_realloc_aligned(arena, ptr, old_len, new_len, align)                                  \
    arena_realloc_raw((arena), (ptr), (old_len) * sizeof(*(ptr)), (new_len) * sizeof(*(ptr)), align)

#define arena_realloc(arena, ptr, old_len, new_len)                                                 \
    arena_realloc_aligned(arena, ptr, old_len, new_len, _Alignof(*(ptr)))

#define arena_free(arena, ptr, len) arena_free_raw((arena), (ptr), (len) * sizeof(*(ptr)))

allocator_t allocator_init_null(void);
allocator_t allocator_init_malloc(void);
allocator_t allocator_init_arena(arena_t* arena);

arena_t arena_init_fixed(void* buffer, usize size);
arena_t arena_init_alloc(allocator_t allocator, usize size);
void arena_release(arena_t* self);
void arena_clear(arena_t* self);
arena_t arena_checkpoint(arena_t* self);
void arena_rollback(arena_t* self, arena_t checkpoint);

void* arena_alloc_raw(arena_t* self, usize size, usize align);
bool arena_resize_raw(arena_t* self, void* ptr, usize old_size, usize new_size);
void* arena_realloc_raw(arena_t* self, void* ptr, usize old_size, usize new_size, usize align);
void arena_free_raw(arena_t* self, void* ptr, usize size);

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
