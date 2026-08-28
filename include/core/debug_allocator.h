#pragma once
#include "allocator.h"
#include "hashmap.h"

typedef struct allocation_info {
    usize size;
    u32   align;
    bool  alive;
} allocation_info;

HASHMAP_DECL_RAW(void*, allocation_info, map_alloc_info)

typedef struct debug_allocator {
    allocator_t    _backing;
    map_alloc_info _allocations;
    i32            fail_after_n;
    bool           force_resize_fail;
} debug_allocator;

debug_allocator debug_allocator_init(allocator_t internal, allocator_t backing);
bool debug_allocator_release(debug_allocator* debug, bool log_leaks);

allocator_t allocator_init_debug(debug_allocator* debug);
