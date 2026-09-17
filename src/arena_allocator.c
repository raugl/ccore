#include <core/arena_allocator.h>
#include <core/tlsf_allocator.h>

typedef struct arena_debug_header {
    u32 size;
    u32 span;
} arena_debug_header;

#ifdef DEBUG_ALLOCATOR
#   define DEBUG_CANARY_SIZE  32
#   define DEBUG_HEADER_ALIGN alignof(arena_debug_header)
#   define DEBUG_PREFIX_SIZE  (sizeof(arena_debug_header) + DEBUG_CANARY_SIZE)
#else
#   define DEBUG_CANARY_SIZE  0
#   define DEBUG_HEADER_ALIGN 1
#   define DEBUG_PREFIX_SIZE  0
#endif

FORCE_INLINE bool has_only_head_chunk(arena_t self) {
    assert((self._append_ptr == NULL) == (self._tail_block == INVALID_BLOCK));

    if (self._tail_block == INVALID_BLOCK) return false;
    return self._head_block == self._tail_block;
}

FORCE_INLINE bool is_head_chunk_fixed(arena_t self) {
    assert((self._append_ptr == NULL) == (self._tail_block == INVALID_BLOCK));

    if (self._tail_block == INVALID_BLOCK) return false;
    return _ccore_global_tlsf->blocks[self._head_block].flags & TLSF_BLOCK_EXTERNAL;
}

FORCE_INLINE bool has_non_fixed_tail_chunk(arena_t self) {
    assert((self._append_ptr == NULL) == (self._tail_block == INVALID_BLOCK));

    if (self._tail_block == INVALID_BLOCK) return false;
    if (_ccore_global_tlsf->blocks[self._tail_block].flags & TLSF_BLOCK_EXTERNAL) {
        assert(self._head_block == self._tail_block);
        return false;
    }
    return true;
}

FORCE_INLINE tlsf_block* get_head_chunk(arena_t self) {
    assert(self._append_ptr != NULL);
    assert(self._head_block != INVALID_BLOCK);
    assert(self._tail_block != INVALID_BLOCK);

    return &_ccore_global_tlsf->blocks[self._head_block];
}

static void assert_canaries(arena_debug_header* header) {
    enum { NO_CANARIES = 0, LEFT_CANARY = 1, RIGHT_CANARY = 2 };
    u32 sides = 0;

    u8* canary = (u8*)(header + 1);
    for (u32 i = 0; i < DEBUG_CANARY_SIZE; ++i) {
        if (canary[i] != 0xAA) {
            sides += LEFT_CANARY;
            break;
        }
    }
    canary += header->size;
    for (u32 i = 0; i < DEBUG_CANARY_SIZE; ++i) {
        if (canary[i] != 0xAA) {
            sides += RIGHT_CANARY;
            break;
        }
    }
    if (sides != NO_CANARIES) {
        static const cstring bounds[4] = {
            "", "front canary", "back canary", "front and back canaries"
        };
        panic(
            "buffer overrun detected at %p: %s corrupted (size %u)",
            (void*)(header + 1), bounds[sides], header->size
        );
    }
}

static void assert_allocation(arena_t self, u8* old_ptr, usize old_size) {
    tlsf_t* tlsf = _ccore_global_tlsf;

    arena_debug_header* header;
    bool found_chunk = false;
    bool found_allocation = false;

    for (tlsf_index block_index = self._head_block;
         block_index != INVALID_BLOCK;
         block_index = tlsf->blocks[block_index].next_chunk
    ) {
        const tlsf_block* block = &tlsf->blocks[block_index];

        if (block->ptr <= old_ptr && old_ptr < block->ptr + block->size) {
            header = (void*)block->ptr;
            found_chunk = true;

            while ((u8*)header < block->ptr + block->size) {
                if (old_ptr == (u8*)(header + 1)) {
                    found_allocation = true;
                    goto found;
                }
                // NOTE: If the allocation's alignment is less than alignof(arena_debug_header), the
                // header's span might be just short of the next allocation's header. Because of this
                // I just align forward here, instead of the multiple places where I edit the span.
                header = align_forward_ptr((u8*)header + header->span, alignof(arena_debug_header));
            }
        }
    }

found:
    if (!found_chunk) panic(
        "invalid arena pointer %p: pointer is not within an arena chunk", old_ptr
    );
    if (!found_allocation) panic(
        "invalid arena pointer %p: pointer does not reference an allocation", old_ptr
    );
    if (header->size != old_size) panic(
        "mismatched allocation size for %p: expected %u, received %zu",
        (void*)(header + 1), header->size, old_size
    );
    assert_canaries(header);
}

static usize free_and_sum_chunks(arena_t* self) {
    tlsf_t* tlsf = _ccore_global_tlsf;

    if (self->_tail_block == INVALID_BLOCK) {
        return 0;
    }
    u32 total_size = 0;
    tlsf_index block_index = self->_head_block;

    while (block_index != INVALID_BLOCK) {
        const tlsf_block block = tlsf->blocks[block_index];

#ifdef DEBUG_ALLOCATOR
        arena_debug_header* header = (void*)block.ptr;
        while ((u8*)header < block.ptr + block.size) {
            assert_canaries(header);
            header = align_forward_ptr((u8*)header + header->span, alignof(arena_debug_header));
        }
#endif
        tlsf_free_raw(tlsf, block_index);
        block_index = block.next_chunk;
        total_size += block.size;
    }
    return total_size;
}

arena_t arena_init(void) {
    return (arena_t) { 0 };
}

arena_t arena_init_fixed(void* buffer, usize size, bool silence_spillover) {
    assert(buffer != NULL);
    assert(size < UINT32_MAX);

    tlsf_t* tlsf = _ccore_global_tlsf;
    tlsf_index block_index = tlsf_claim_external_block(tlsf, buffer, size);

    return (arena_t) {
        ._append_ptr        = buffer,
        ._avail_size        = (u32)size,
        ._head_block        = block_index,
        ._tail_block        = block_index,
        ._warn_chunk_growth = !silence_spillover,
    };
}

arena_t arena_init_capacity(usize capacity, bool silence_spillover) {
    assert(capacity < UINT32_MAX);

    return (arena_t) {
        ._initial_capacity  = (u16)min_usize(UINT16_MAX, capacity),
        ._warn_chunk_growth = !silence_spillover,
    };
}

void arena_destroy(arena_t* self) {
    assert(self != NULL);
    free_and_sum_chunks(self);
    memset_destroyed(self, sizeof(arena_t));
}

void arena_clear(arena_t* self) {
    assert(self != NULL);

    if (is_head_chunk_fixed(*self)) {
        void* buffer = get_head_chunk(*self)->ptr;
        u32 size = get_head_chunk(*self)->size;
        *self = arena_init_fixed(buffer, size, !self->_warn_chunk_growth);
    } else {
        *self = (arena_t) {
            ._initial_capacity  = (u16)min_usize(UINT16_MAX, free_and_sum_chunks(self)),
            ._warn_chunk_growth = self->_warn_chunk_growth,
        };
    }
}

static bool arena_alloc_reserve(arena_t* self, usize size, usize align);

void* arena_alloc_raw(arena_t* self, usize size, usize align) {
    assert(self != NULL);
    assert(size <= UINT32_MAX);
    assert(align <= BLOCK_ALIGNMENT);
    assert(is_pow2(align));

    align = max_usize(align, DEBUG_HEADER_ALIGN);
    if (!arena_alloc_reserve(self, size, align)) return NULL;

    u8* new_ptr = align_forward_ptr(self->_append_ptr + DEBUG_PREFIX_SIZE, align);
    usize size_bump = (usize)(new_ptr - self->_append_ptr) + size + DEBUG_CANARY_SIZE;

#ifdef DEBUG_ALLOCATOR
    arena_debug_header* header = (void*)(new_ptr - DEBUG_PREFIX_SIZE);
    header->size = (u32)size;
    header->span = (u32)size_bump;
#endif

    memset_undefined(self->_append_ptr, size_bump);
    self->_avail_size -= size_bump;
    self->_append_ptr += size_bump;
    self->_tail_live_allocs++;
    return new_ptr;
}

static bool arena_alloc_reserve(arena_t* self, usize size, usize align) {
    u8* end_ptr = align_forward_ptr(self->_append_ptr + DEBUG_PREFIX_SIZE, align);
    end_ptr += size + DEBUG_CANARY_SIZE;

    bool has_to_grow = self->_append_ptr + self->_avail_size < end_ptr;
    if (!has_to_grow) return true;

#ifdef DEBUG_ALLOCATOR
        if (has_only_head_chunk(*self) && self->_warn_chunk_growth) {
            log_warn(
                "Arena exceeded the initial capacity of its%s head chunk (%d).\n",
                is_head_chunk_fixed(*self) ? " fixed" : "", get_head_chunk(*self)->size
            );
        }
        if (self->_fail_alloc || self->_fail_everything) return false;
#endif

    tlsf_t* tlsf = _ccore_global_tlsf;
    usize min_size, desired_size, chunk_size;

    if (self->_tail_block != INVALID_BLOCK) {
        tlsf_block* tail_chunk = &tlsf->blocks[self->_tail_block];
        desired_size = tail_chunk->size + (tail_chunk->size >> 1);

        min_size = (usize)(end_ptr - tail_chunk->ptr);
        chunk_size = tlsf_get_policy_suggested_chunk_size(tlsf, min_size, desired_size);

        if (tlsf_resize_raw(tlsf, self->_tail_block, chunk_size)) {
            memset_undefined(self->_append_ptr, chunk_size - tail_chunk->size);
            self->_avail_size += chunk_size - tail_chunk->size;
            return true;
        }
    } else {
        desired_size = self->_initial_capacity;
    }

    min_size = align_forward(DEBUG_PREFIX_SIZE, align) + size + DEBUG_CANARY_SIZE;
    chunk_size = tlsf_get_policy_suggested_chunk_size(tlsf, min_size, desired_size);

    tlsf_allocation allocation;
    if (!tlsf_alloc_raw(tlsf, chunk_size, &allocation)) return false;

    if (self->_tail_block != INVALID_BLOCK) {
        tlsf->blocks[self->_tail_block].next_chunk = allocation.block_index;
    } else {
        self->_head_block = allocation.block_index;
    }
    self->_append_ptr = allocation.ptr;
    self->_avail_size = allocation.size;
    self->_tail_block = allocation.block_index;
    self->_tail_live_allocs = 0;
    return true;
}

bool arena_resize_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size) {
    assert(self != NULL);
    assert(old_ptr != NULL);
    assert(old_size != 0);
    assert(old_size <= UINT32_MAX);
    assert(new_size <= UINT32_MAX);
    assert(self->_append_ptr != NULL);
    assert(self->_tail_block != INVALID_BLOCK);
    assert(self->_tail_live_allocs >= 1);

#ifdef DEBUG_ALLOCATOR
    assert_allocation(*self, old_ptr, old_size);
    arena_debug_header* header = (void*)((u8*)old_ptr - DEBUG_PREFIX_SIZE);

    if (new_size == 0) {
        log_warn("Trying to 'free' an arena allocation by resizing it to zero");
    }
    if (self->_fail_resize || self->_fail_everything) return false;
#endif

    tlsf_t* tlsf = _ccore_global_tlsf;
    isize size_diff = (isize)new_size - (isize)old_size;
    bool is_last = (u8*)old_ptr + old_size + DEBUG_CANARY_SIZE == self->_append_ptr;

    if (size_diff < 0) {
#ifdef DEBUG_ALLOCATOR
        header->size += size_diff;
        memset_undefined((u8*)old_ptr + new_size, DEBUG_CANARY_SIZE);
#endif
        memset_destroyed((u8*)old_ptr + new_size + DEBUG_CANARY_SIZE, old_size - new_size - DEBUG_CANARY_SIZE);
    }
    if (!is_last) {
        return size_diff <= 0;
    }
    if (size_diff > self->_avail_size) {
        tlsf_block* tail_chunk = &tlsf->blocks[self->_tail_block];
        usize desired_size = tail_chunk->size + (tail_chunk->size >> 1);

        usize min_size = (usize)(self->_append_ptr + size_diff - tail_chunk->ptr);
        usize chunk_size = tlsf_get_policy_suggested_chunk_size(tlsf, min_size, desired_size);

        if (!tlsf_resize_raw(tlsf, self->_tail_block, chunk_size)) return false;
        memset_undefined(self->_append_ptr, chunk_size - tail_chunk->size);
        self->_avail_size += chunk_size - tail_chunk->size;
    }

#ifdef DEBUG_ALLOCATOR
    header->size += size_diff;
    header->span += size_diff;
#endif
    self->_append_ptr += size_diff;
    self->_avail_size -= size_diff;
    return true;
}

// This implements both the conventional aliases for alloc and free, but also frees the last chunk if
// it only held the allocation to be realloc'd and it wasn't able to resize it to fit the new size.
void* arena_realloc_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size, usize align) {
    assert(self != NULL);
    assert(old_size <= UINT32_MAX);
    assert(new_size <= UINT32_MAX);
    assert(align <= BLOCK_ALIGNMENT);
    assert(is_pow2(align));

    if (old_ptr == NULL || old_size == 0) {
        assert(old_ptr == NULL && old_size == 0);
        return arena_alloc_raw(self, new_size, align);
    }
    if (new_size == 0) {
        assert(arena_resize_raw(self, old_ptr, old_size, 0));
        return NULL;
    }
    if (arena_resize_raw(self, old_ptr, old_size, new_size)) {
        return old_ptr;
    }
    assert(new_size > old_size);

    arena_replace_info info;
    if (arena_begin_replace_raw(self, old_ptr, old_size, new_size, align, &info)) {
        memcpy(info.ptr, old_ptr, old_size);
        arena_commit_replace(self, info);
        return info.ptr;
    }
    return NULL;
}

bool arena_begin_replace_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size, usize align, arena_replace_info* out_info) {
    assert(out_info != NULL);
    bool is_last = (u8*)old_ptr + old_size + DEBUG_CANARY_SIZE == self->_append_ptr;

    *out_info = (arena_replace_info) {
        .size = (u32)new_size,
        ._prev_tail = self->_tail_block,
        ._free_prev = has_non_fixed_tail_chunk(*self) && (self->_tail_live_allocs <= 1) && is_last,
    };

    out_info->ptr = arena_alloc_raw(self, new_size, align);
    if (out_info->ptr == NULL) {
        *out_info = (arena_replace_info) { 0 };
        return false;
    }

    if (self->_tail_block == out_info->_prev_tail) {
        out_info->_free_prev = false;
        self->_tail_live_allocs--;
    }
    return true;
}

void arena_commit_replace(arena_t* self, arena_replace_info info) {
    assert(self != NULL);
    tlsf_t* tlsf = _ccore_global_tlsf;

    if (info._free_prev) {
        assert(info._prev_tail != self->_tail_block);
        if (self->_head_block == info._prev_tail) {
            self->_head_block = tlsf->blocks[info._prev_tail].next_chunk;
        }
        tlsf_free_raw(tlsf, info._prev_tail);
    }
}
