#pragma once
#include "common.h"

// TODO
#define DEBUG_ALLOCATOR 1

typedef u16 tlsf_index;

typedef struct arena_t {
    u8*        _append_ptr;
    u32        _avail_size;
    tlsf_index _tail_block;
    union {
        tlsf_index _head_block;
        // TODO: i've got 2 space bytes, and thats before trying to compact the bools into a bit
        // field. I should use them to promote _initial_capacity to a u32 and not loose so much
        // capacity on clear.
        u16 _initial_capacity;
    };
    u16        _tail_live_allocs;
    bool       _fail_alloc;
    bool       _fail_resize;
    bool       _fail_everything;
    bool       _warn_chunk_growth;
} arena_t;

typedef struct arena_replacement_info {
    void*      ptr;
    u32        size;
    tlsf_index _prev_tail;
    bool       _free_prev;
} arena_replace_info;

arena_t arena_init(void);
arena_t arena_init_fixed(void* buffer, usize size, bool silence_spillover);
arena_t arena_init_capacity(usize capacity, bool silence_spillover);
void arena_destroy(arena_t* self);
void arena_clear(arena_t* self);

bool arena_begin_replace_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size, usize align, arena_replace_info* out_info);
void arena_commit_replace(arena_t* self, arena_replace_info info);

void* arena_alloc_raw(arena_t* self, usize size, usize align);
bool arena_resize_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size);
void* arena_realloc_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size, usize align);

#define arena_alloc(arena, T, len)                                                                  \
    arena_alloc_raw((arena), (len) * sizeof(T), alignof(T))

#define arena_alloc_aligned(arena, T, len, align)                                                   \
    arena_alloc_raw((arena), (len) * sizeof(T), (align))

#define arena_resize(arena, old_ptr, old_len, new_len)                                              \
    arena_resize_raw((arena), (old_ptr), (old_len) * sizeof(*(old_ptr)), (new_len) * sizeof(*(old_ptr)))

#define arena_realloc(arena, old_ptr, old_len, new_len)                                             \
    arena_realloc_raw((arena), (old_ptr), (old_len) * sizeof(*(old_ptr)), (new_len) * sizeof(*(old_ptr)), alignof_expr(*(old_ptr)))

#define arena_realloc_aligned(arena, old_ptr, old_len, new_len, align)                              \
    arena_realloc_raw((arena), (old_ptr), (old_len) * sizeof(*(old_ptr)), (new_len) * sizeof(*(old_ptr)), align)

#define arena_begin_replace(arena, old_ptr, old_len, new_len, out_info)                             \
    arena_begin_replace_raw((arena), (old_ptr), (old_len) * sizeof(*(old_ptr)), (new_len) * sizeof(*(old_ptr)), alignof_expr(*(old_ptr)), (out_info))
