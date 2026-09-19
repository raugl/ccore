#include <core/math.h>
#include <core/arena_allocator.h>
#include <core/tlsf_allocator.h>

#ifdef DEBUG_ALLOCATOR
#   define DEBUG_CANARY_SIZE  32
#   define DEBUG_HEADER_ALIGN alignof(arena_debug_header)
#   define DEBUG_PREFIX_SIZE  (sizeof(arena_debug_header) + DEBUG_CANARY_SIZE)
#else
#   define DEBUG_CANARY_SIZE  0
#   define DEBUG_HEADER_ALIGN 1
#   define DEBUG_PREFIX_SIZE  0
#endif

typedef struct arena_debug_header {
    u32 size;        // The up to date size of the allocation past the header
    u32 next_offset; // The typed offset (not bytes) to the next header, 0/TLSF_NIL_INDEX in case there is none
} arena_debug_header;

FORCE_INLINE tlsf_block* get_tail_chunk(arena_t self) {
    tlsf_t* tlsf = _ccore_global_tlsf;

    assert(tlsf != NULL);
    assert(self._tail_chunk < tlsf->metadata_capacity);
    assert((self._append_ptr == NULL) == (self._tail_chunk == TLSF_NIL_INDEX));

    if (self._tail_chunk == TLSF_NIL_INDEX) return NULL;
    tlsf_block* tail = &tlsf_blocks(tlsf)[self._tail_chunk];
    assert(tail->flags == TLSF_BLOCK_ALLOCATED);
    return tail;
}

FORCE_INLINE tlsf_block* get_head_chunk_if_single(arena_t self) {
    tlsf_block* tail = get_tail_chunk(self);
    return (tail != NULL && tail->prev_chunk == TLSF_NIL_INDEX) ? tail : NULL;
}

FORCE_INLINE bool has_non_fixed_tail_chunk(arena_t self) {
    const tlsf_block* tail = get_tail_chunk(self);
    if (tail == NULL) return false;

    if (tail->kind == TLSF_BLOCK_EXTERNAL) {
        assert(tail->prev_chunk == TLSF_NIL_INDEX); // Only the head chunk can be fixed
        return false;
    }
    return true;
}

#ifdef DEBUG_ALLOCATOR
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
    assert(tlsf != NULL);

    bool found_chunk = false;
    arena_debug_header* header = NULL;

    for (tlsf_index chunk_index = self._tail_chunk;
         chunk_index != TLSF_NIL_INDEX;
         chunk_index = tlsf_blocks(tlsf)[chunk_index].prev_chunk
    ) {
        const tlsf_block chunk = tlsf_blocks(tlsf)[chunk_index];
        assert(chunk_index < tlsf->metadata_capacity);
        assert(chunk.flags == TLSF_BLOCK_ALLOCATED);

        if (chunk.ptr <= old_ptr && old_ptr < chunk.ptr + chunk.size) {
            // This exists becaues the header is aligned to 4 bytes, but the fixed buffer could have
            // any alignment. The header is always inserted at its correct alignment, so to retrieve
            // the first header inside a chunk, i just need to align forward the chunk's pointer. All
            // chunks will have an activie initial header, because otherwise replaced would have just
            // freed them.
            header = align_forward_ptr(chunk.ptr, alignof(arena_debug_header));
            found_chunk = true;

            while (header->next_offset != TLSF_NIL_INDEX) {
                if (old_ptr == (void*)(header + 1)) {
                    goto found;
                }
                header += header->next_offset;
            }
        }
    }

found:
    if (!found_chunk) panic(
        "invalid arena pointer %p: pointer is not within an arena chunk", old_ptr
    );
    if (header == NULL) panic(
        "invalid arena pointer %p: pointer does not reference an allocation", old_ptr
    );
    if (header->size != old_size) panic(
        "mismatched allocation size for %p: expected %u, received %zu",
        (void*)(header + 1), header->size, old_size
    );
    assert_canaries(header);
}
#endif

typedef struct free_chunks_result {
    u64 total_size;
    u8* fixed_buffer;
    u32 fixed_size;
} cleanup_chunks_result;

static cleanup_chunks_result cleanup_chunks(arena_t self) {
    tlsf_t* tlsf = _ccore_global_tlsf;
    assert(tlsf != NULL);

    bool found_fixed = false;
    cleanup_chunks_result result = {0};

    for (tlsf_index chunk_index = self._tail_chunk; chunk_index != TLSF_NIL_INDEX;) {
        const tlsf_block chunk = tlsf_blocks(tlsf)[self._tail_chunk];
        assert(chunk_index < tlsf->metadata_capacity);
        assert(chunk.flags == TLSF_BLOCK_ALLOCATED);

        if (chunk.kind == TLSF_BLOCK_EXTERNAL) {
            assert(!found_fixed);
            assert(chunk.size > 0);
            assert(chunk.ptr != NULL);

            result.fixed_buffer = chunk.ptr;
            result.fixed_size = chunk.size;
            found_fixed = true;

#ifdef DEBUG_ALLOCATOR
            // This exists becaues the header is aligned to 4 bytes, but the fixed buffer could have
            // any alignment. The header is always inserted at its correct alignment, so to retrieve
            // the first header inside a chunk, i just need to align forward the chunk's pointer. All
            // chunks will have an activie initial header, because otherwise replaced would have just
            // freed them.
            arena_debug_header* header = align_forward_ptr(chunk.ptr, alignof(arena_debug_header));

            while (header->next_offset != TLSF_NIL_INDEX) {
                assert_canaries(header);
                header += header->next_offset;
            }
#endif
            tlsf_free_block(tlsf, chunk_index);
            result.total_size += chunk.size;
            chunk_index = chunk.prev_chunk;
        }
    }
    return result;
}

arena_t arena_init(void) {
    return (arena_t){0};
}

// Will panic if TLSF metadata capacity is all used up. That is a failed inital configuration
// assumption, not something worth defending against every time you use an arena.
arena_t arena_init_fixed(void* buffer, usize size, bool silence_spillover) {
    tlsf_t* tlsf = _ccore_global_tlsf;

    assert(tlsf != NULL);
    assert(buffer != NULL);
    assert(size <= UINT32_MAX);

    return (arena_t) {
        ._append_ptr     = buffer,
        ._avail_size     = (u32)size,
        ._tail_chunk     = tlsf_claim_external_block(tlsf, buffer, size),
        ._warn_spillover = !silence_spillover,
    };
}

arena_t arena_init_capacity(usize capacity, bool silence_spillover) {
    assert(capacity <= UINT32_MAX);

    return (arena_t) {
        ._initial_capacity  = (u16)min_usize(UINT16_MAX, capacity),
        ._warn_spillover = !silence_spillover,
    };
}

void arena_destroy(arena_t* self) {
    assert(self != NULL);
    cleanup_chunks(*self);
    memset_destroyed(self, sizeof(arena_t));
}

void arena_clear(arena_t* self) {
    assert(self != NULL);
    cleanup_chunks_result result = cleanup_chunks(*self);

    if (result.fixed_buffer != NULL) {
        *self = arena_init_fixed(result.fixed_buffer, result.fixed_size, !self->_warn_spillover);
    } else {
        *self = (arena_t) {
            ._initial_capacity  = (u32)min_u64(UINT32_MAX, result.total_size),
            ._warn_spillover = self->_warn_spillover,
        };
    }
}

static bool arena_alloc_reserve(arena_t* self, usize size, usize align);

void* arena_alloc_raw(arena_t* self, usize size, usize align) {
    assert(self != NULL);
    assert(size <= UINT32_MAX);
    assert(align <= TLSF_BLOCK_ALIGNMENT);
    assert(is_pow2(align));

    static_assert(DEBUG_PREFIX_SIZE % alignof(arena_debug_header) == 0, "");
    align = max_usize(align, DEBUG_HEADER_ALIGN);
    if (!arena_alloc_reserve(self, size, align)) return NULL;

    u8* new_ptr = align_forward_ptr(self->_append_ptr + DEBUG_PREFIX_SIZE, align);
    usize size_bump = (usize)(new_ptr - self->_append_ptr) + size + DEBUG_CANARY_SIZE;

#ifdef DEBUG_ALLOCATOR
    arena_debug_header* curr_header = (void*)(new_ptr - DEBUG_PREFIX_SIZE);
    arena_debug_header* prev_header = (void*)(self->_append_ptr - self->_prev_alloc_size);

    if (self->_prev_alloc_size != 0) {
        prev_header->next_offset = (u32)(curr_header - prev_header);
    }
    curr_header->size = (u32)size;
    self->_prev_alloc_size = (u32)(DEBUG_PREFIX_SIZE + size + DEBUG_CANARY_SIZE);
#endif

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
    tlsf_block* head = get_head_chunk_if_single(*self);

    if (head != NULL && self->_warn_spillover) log_warn(
        "Arena exceeded the initial capacity of its%s head chunk (%d).\n",
        head->kind == TLSF_BLOCK_EXTERNAL ? " fixed" : "", head->size
    );
    if (self->_fail_alloc || self->_fail_everything) return false;
#endif

    tlsf_t* tlsf = _ccore_global_tlsf;
    usize desired_size = self->_initial_capacity;

    if (self->_tail_chunk != TLSF_NIL_INDEX) {
        const tlsf_block tail_chunk = tlsf_blocks(tlsf)[self->_tail_chunk];
        desired_size = tail_chunk.size + (tail_chunk.size >> 1);

        usize min_size = (usize)(end_ptr - tail_chunk.ptr);
        u32 chunk_size = tlsf_get_policy_suggested_chunk_size(tlsf, min_size, desired_size);

        if (tlsf_resize_block(tlsf, self->_tail_chunk, chunk_size)) {
            self->_avail_size += chunk_size - tail_chunk.size;
            return true;
        }
    }

    static_assert(TLSF_BLOCK_ALIGNMENT > alignof(arena_debug_header), "");
    usize min_size = align_forward(DEBUG_PREFIX_SIZE, align) + size + DEBUG_CANARY_SIZE;
    u32 chunk_size = tlsf_get_policy_suggested_chunk_size(tlsf, min_size, desired_size);

    tlsf_allocation allocation;
    if (!tlsf_alloc_block(tlsf, chunk_size, &allocation)) return false;

    if (self->_tail_chunk != TLSF_NIL_INDEX) {
        tlsf_blocks(tlsf)[allocation.block_index].prev_chunk = self->_tail_chunk;
    }
    self->_append_ptr = allocation.ptr;
    self->_avail_size = allocation.size;
    self->_tail_chunk = allocation.block_index;
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
    assert(self->_tail_chunk != TLSF_NIL_INDEX);
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
    const isize size_diff = (isize)new_size - (isize)old_size;
    const bool is_last = (u8*)old_ptr + old_size + DEBUG_CANARY_SIZE == self->_append_ptr;

    if (size_diff < 0) {
#ifdef DEBUG_ALLOCATOR
        header->size += size_diff;
        memset_undefined((u8*)old_ptr + new_size, DEBUG_CANARY_SIZE);
#endif
        memset_destroyed((u8*)old_ptr + new_size + DEBUG_CANARY_SIZE, old_size - new_size - DEBUG_CANARY_SIZE);
    }
    if (!is_last) return size_diff <= 0;

    if (size_diff > self->_avail_size) {
        const tlsf_block tail_chunk = tlsf_blocks(tlsf)[self->_tail_chunk];
        usize desired_size = tail_chunk.size + (tail_chunk.size >> 1);

        usize min_size = tail_chunk.size - self->_avail_size + (usize)size_diff;
        u32 chunk_size = tlsf_get_policy_suggested_chunk_size(tlsf, min_size, desired_size);

        if (!tlsf_resize_block(tlsf, self->_tail_chunk, chunk_size)) return false;
        self->_avail_size += chunk_size - tail_chunk.size;
    }

#ifdef DEBUG_ALLOCATOR
    header->size += size_diff;
#endif
    self->_append_ptr += size_diff;
    self->_avail_size -= size_diff;
    return true;
}

void* arena_realloc_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size, usize align) {
    assert(self != NULL);
    assert(old_size <= UINT32_MAX);
    assert(new_size <= UINT32_MAX);
    assert(align <= TLSF_BLOCK_ALIGNMENT);
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

// TODO: add more asserts, basically copy everything from realloc as its the same api in practice
// TODO: also fix the deque, its not using this properly. actually rethink the whole usage of
// replace, i think i might want to turn it into some customizable realloc and get rid of resize
bool arena_begin_replace_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size, usize align, arena_replace_info* out_info) {
    assert(self != NULL);
    assert(out_info != NULL);
    assert(old_size <= UINT32_MAX);
    assert(new_size <= UINT32_MAX);
    assert(align <= TLSF_BLOCK_ALIGNMENT);
    assert(is_pow2(align));

    bool is_last = self->_append_ptr == (u8*)old_ptr + old_size + DEBUG_CANARY_SIZE;

    *out_info = (arena_replace_info) {
        .size = (u32)new_size,
        ._prev_tail = self->_tail_chunk,
        ._free_prev = has_non_fixed_tail_chunk(*self) && (self->_tail_live_allocs <= 1) && is_last,
    };

    out_info->ptr = arena_alloc_raw(self, new_size, align);
    if (out_info->ptr == NULL) {
        *out_info = (arena_replace_info) { 0 };
        return false;
    }

    if (self->_tail_chunk == out_info->_prev_tail) {
        out_info->_free_prev = false;
        self->_tail_live_allocs--;
    }
    return true;
}

void arena_commit_replace(arena_t* self, arena_replace_info info) {
    assert(self != NULL);
    tlsf_t* tlsf = _ccore_global_tlsf;

    if (info._free_prev) {
        assert(info._prev_tail != self->_tail_chunk);
        tlsf_blocks(tlsf)[self->_tail_chunk].prev_chunk = tlsf_blocks(tlsf)[info._prev_tail].prev_chunk;
        tlsf_free_block(tlsf, info._prev_tail);
    }
}
