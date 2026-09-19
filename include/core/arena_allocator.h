#pragma once
#include "common.h"

typedef u16 tlsf_index;

typedef struct arena_t {
    u8*        _append_ptr;
    u32        _avail_size;
    union {                       // tagged by _tail_chunk == TLSF_NIL_INDEX
        u32    _initial_capacity; // the initial chunk size, when there is no tail
        u32    _prev_alloc_size;  // the useful size of the previous allocation i.e. prefix + size + sufix but with no align padding, only used in DEBUG_ALLOCATOR mode
    };
    tlsf_index _tail_chunk;
    u16        _tail_live_allocs;
    bool       _fail_alloc;
    bool       _fail_resize;
    bool       _fail_everything;
    bool       _warn_spillover;
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
