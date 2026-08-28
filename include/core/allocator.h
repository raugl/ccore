#pragma once
#include "common.h"
#include "math.h"

typedef enum allocator_operation {
    ALLOCATOR_OPERATION_ALLOC = 0,
    ALLOCATOR_OPERATION_REALLOC,
    ALLOCATOR_OPERATION_RESIZE,
    ALLOCATOR_OPERATION_FREE,
} allocator_operation;

typedef void* (*allocator_proc)(
    allocator_operation op,
    void* self,
    void* old_ptr,
    usize old_size,
    usize new_size,
    usize align
);

typedef struct allocator_t {
    void*          self;
    allocator_proc proc;
} allocator_t;

#define mem_alloc_aligned(alloc, T, len, align)                                                     \
    (alloc).proc(ALLOCATOR_OPERATION_ALLOC, (alloc).self, 0, 0, (len) * sizeof(T), align)

#define mem_alloc(alloc, T, len) mem_alloc_aligned(alloc, T, len, _Alignof(T))

#define mem_resize(alloc, old_ptr, old_len, new_len)                                                \
    ((alloc).proc(                                                                                  \
        ALLOCATOR_OPERATION_RESIZE,                                                                 \
        (alloc).self,                                                                               \
        (old_ptr),                                                                                  \
        (old_len) * sizeof(*(old_ptr)),                                                             \
        (new_len) * sizeof(*(old_ptr)),                                                             \
        0                                                                                           \
    ) != NULL)

#define mem_realloc_aligned(alloc, old_ptr, old_len, new_len, align)                                \
    (alloc).proc(                                                                                   \
        ALLOCATOR_OPERATION_REALLOC,                                                                \
        (alloc).self,                                                                               \
        (old_ptr),                                                                                  \
        (old_len) * sizeof(*(old_ptr)),                                                             \
        (new_len) * sizeof(*(old_ptr)),                                                             \
        align                                                                                       \
    )

#define mem_realloc(alloc, old_ptr, old_len, new_len)                                               \
    mem_realloc_aligned(alloc, old_ptr, old_len, new_len, alignof_expr(*(old_ptr)))

#define mem_free(alloc, old_ptr, len)                                                               \
    (alloc).proc(ALLOCATOR_OPERATION_FREE, (alloc).self, (old_ptr), (len) * sizeof(*(old_ptr)), 0, 0)

typedef struct arena_t {
    u8*         ptr;
    allocator_t allocator;
    u32         offset, capacity;
} arena_t;

#define arena_alloc(arena, T, len) arena_alloc_aligned(arena, T, len, _Alignof(T))

#define arena_alloc_aligned(arena, T, len, align) arena_alloc_raw((arena), len * sizeof(T), align)

#define arena_resize(arena, old_ptr, old_len, new_len)                                              \
    arena_resize_raw((arena), (old_ptr), (old_len) * sizeof(*(old_ptr)), (new_len) * sizeof(*(old_ptr)))

#define arena_realloc_aligned(arena, old_ptr, old_len, new_len, align)                              \
    arena_realloc_raw((arena), (old_ptr), (old_len) * sizeof(*(old_ptr)), (new_len) * sizeof(*(old_ptr)), align)

#define arena_realloc(arena, old_ptr, old_len, new_len)                                             \
    arena_realloc_aligned(arena, old_ptr, old_len, new_len, alignof_expr(*(old_ptr)))

#define arena_free(arena, old_ptr, len) arena_free_raw((arena), (old_ptr), (len) * sizeof(*(old_ptr)))

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
bool arena_resize_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size);
void* arena_realloc_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size, usize align);
void arena_free_raw(arena_t* self, void* old_ptr, usize old_size);

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
