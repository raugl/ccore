#include <core/tlsf_allocator.h>
#include <core/math.h>

FORCE_INLINE bool is_block_free(const tlsf_t* self, tlsf_index block_index) {
    assert(self != NULL);
    return (tlsf_blocks(self)[block_index].flags & TLSF_BLOCK_ALLOCATED) == 0;
}

FORCE_INLINE usize round_up_block_size(usize size) {
    return (size + MIN_BLOCK_SIZE - 1) & (MIN_BLOCK_SIZE - 1);
}

// Returns the index of the first set bit which is >= start. If not found, returns 0.
FORCE_INLINE u16 find_first_set_after(u32 bits, u16 start) {
    u32 mask = (1u << start) - 1;
    u32 bits_after = bits & ~mask;
    if (bits_after == 0) {
        return 0;
    }
    return (u16)__builtin_ctz(bits_after);
}

// 336 total bins, 21 top level, 16 bottom level, 256 as the smallest size
FORCE_INLINE u16 map_size_to_bin(u32 size, bool round_up) {
    if (size < 256) return 0;
    if (size < 4096) return (u16)(size >> 8) - 1;

    u32 shift = log2_u32(size) - 4;
    u32 top_index = log2_u32(size) - 11;
    u32 bottom_index = (size >> shift) & 15;
    u32 round_up_mask = (1U << shift) - 1;

    if (round_up && (size & round_up_mask)) {
        bottom_index++;
    }
    return (u16)((top_index << 4) + bottom_index - 1);
}

FORCE_INLINE u32 map_bin_to_size(u16 bin_index) {
    u32 top_index = (bin_index + 1) >> 4;
    u32 bottom_index = (bin_index + 1) & 15;
    if (top_index == 0) {
        return bottom_index << 8;
    }
    u32 log2 = top_index + 11;
    return (1u << log2) + (bottom_index << (log2 - 4));
}

FORCE_INLINE tlsf_index claim_block_index(tlsf_t* self) {
    // NOTE: You must make sure that there is sufficient metadata capacity before calling this
    // function. This can be achieved by calling one of `reserve_spare_metadata(self, 1)` or
    // `remove_free_block` at any level, before this. This is so because it's a lot easier to
    // ensure sufficient storage before starting any allocator state changes, than trying to roll
    // those back after failing durring them.
    assert(self->block_pool_len > 0);
    return tlsf_block_pool(self)[--self->block_pool_len];
}

FORCE_INLINE void unclaim_block_index(tlsf_t* self, tlsf_index block_index) {
    // NOTE: I don't need to `reserve_spare_metadata` here because I'm just putting back elements
    // which were previously popped from the same array. Short of potential future metadata
    // shrinking, trying to put back more things that you got out is certantly an error.
    assert(self->block_pool_len < self->metadata_capacity);
    assert(block_index != INVALID_BLOCK);
    tlsf_block_pool(self)[self->block_pool_len++] = block_index;
}

// Insert a slice of memory as a free block into the appropriate bin's linked list.
static void insert_free_block(tlsf_t* self, void* buffer, u32 size, tlsf_block_kind kind, tlsf_index prev_phys, tlsf_index next_phys) {
    assert(self != NULL);
    assert(buffer != NULL);
    assert(size <= UINT32_MAX);
    assert(size % MIN_BLOCK_SIZE == 0);
    assert(is_aligned_ptr(buffer, BLOCK_ALIGNMENT));
    assert(kind == TLSF_BLOCK_NORMAL || kind == TLSF_BLOCK_FIXED);

    const u16 bin_index = map_size_to_bin(size, false);
    const u16 top_index = bin_index >> 4;
    const u16 bottom_index = bin_index & 15;

    self->top_bins |= 1u << top_index;
    self->bottom_bins[top_index] |= 1u << bottom_index;
    self->free_size += size;

    const tlsf_index head_index = tlsf_bin_heads(self)[bin_index];
    const tlsf_index block_index = claim_block_index(self);

    tlsf_blocks(self)[block_index] = (tlsf_block) {
        .ptr       = buffer,
        .size      = size,
        .kind      = kind,
        .flags     = TLSF_BLOCK_HEAD | TLSF_BLOCK_CLAIMED,
        .prev_free = bin_index,
        .next_free = head_index,
        .prev_phys = prev_phys,
        .next_phys = next_phys,
    };
    if (head_index != INVALID_BLOCK) {
        tlsf_block* old_head = &tlsf_blocks(self)[head_index];
        old_head->prev_free = block_index;
        old_head->flags &= ~TLSF_BLOCK_HEAD;
    }
    if (prev_phys != INVALID_BLOCK) {
        tlsf_blocks(self)[prev_phys].next_phys = block_index;
    }
    if (next_phys != INVALID_BLOCK) {
        tlsf_blocks(self)[next_phys].prev_phys = block_index;
    }
    tlsf_bin_heads(self)[bin_index] = block_index;
}

static void remove_free_block(tlsf_t* self, tlsf_index block_index) {
    assert(self != NULL);
    assert(0 < block_index);
    assert(block_index < self->metadata_capacity);

    tlsf_block* block = &tlsf_blocks(self)[block_index];
    self->free_size -= block->size;

    assert(block->kind != TLSF_BLOCK_SPARE);
    assert(block->kind != TLSF_BLOCK_EXTERNAL);
    assert(block->flags & TLSF_BLOCK_CLAIMED);
    assert((block->flags & TLSF_BLOCK_ALLOCATED) == 0);

    if (block->flags & TLSF_BLOCK_HEAD) {
        const u16 bin_index = block->prev_free;
        const u16 top_index = bin_index >> 4;
        const u16 bottom_index = bin_index & 15;

        tlsf_bin_heads(self)[bin_index] = block->next_free;
        block->flags &= ~TLSF_BLOCK_HEAD;

        if (block->next_free != INVALID_BLOCK) {
            tlsf_block* new_head = &tlsf_blocks(self)[block->next_free];
            new_head->prev_free = bin_index;
            new_head->flags |= TLSF_BLOCK_HEAD;
        } else {
            // The bottom bin is now empty, zero its corresponding bit
            self->bottom_bins[top_index] &= ~(1u << bottom_index);

            // If all bottom bins are now empty, zero the corresponding top bit
            if (self->bottom_bins[top_index] == 0) {
                self->top_bins &= ~(1u << top_index);
            }
        }
    } else {
        tlsf_blocks(self)[block->prev_free].next_free = block->next_free;

        if (block->next_free != INVALID_BLOCK) {
            tlsf_blocks(self)[block->next_free].prev_free = block->prev_free;
        }
    }
    unclaim_block_index(self, block_index);
    *block = (tlsf_block){ 0 };
}

static bool reserve_new_backing_region(tlsf_t* self) {
    assert(self != NULL);
    assert((self->next_region_size & MIN_BLOCK_SIZE) == 0);

    const u32 region_size = self->next_region_size;
    if (self->next_region_size == UINT32_MAX) return false; // Size overflow
    if (self->block_pool_len == 0) return false; // Out of metadata memory

    u8* new_region = aligned_alloc(BLOCK_ALIGNMENT, region_size);
    if (new_region == NULL) {
        return false; // Out of backing memory
    }
    insert_free_block(self, new_region, region_size, TLSF_BLOCK_NORMAL, INVALID_BLOCK, INVALID_BLOCK);
    self->backing_size += region_size;
    self->free_size += region_size;

    assert(self->backing_regions_len < array_len(self->backing_regions));
    self->backing_regions[self->backing_regions_len++] = new_region;


    self->next_region_size += self->next_region_size >> 1;
    if (self->next_region_size < region_size) {
        self->next_region_size = UINT32_MAX;
    }
    return true;
}

// Returns a structure containing the amount of free space remaining, as well as the
// largest amount that can be allocated at once.
tlsf_storage_report tlsf_get_storage_report(tlsf_t self) {
    tlsf_storage_report report = {
        .free_size = self.free_size,
    };
    if (report.free_size > 0 && self.top_bins != 0) {
        const u32 top_index = log2_u32(self.top_bins);
        const u32 bottom_index = log2_u32(self.bottom_bins[top_index]);
        const u32 bin_index = (top_index << 4) + bottom_index;

        report.largest_free_block = map_bin_to_size((u16)bin_index);
        assert(report.largest_free_block <= report.free_size);
    }
    return report;
}

// Returns detailed information about the number of allocations in each bin.
tlsf_storage_report_full tlsf_get_storage_report_full(tlsf_t self) {
    tlsf_storage_report_full report = {0};

    for (u16 bin_index = 0; bin_index < 320; ++bin_index) {
        u32 block_index = tlsf_bin_heads(&self)[bin_index];

        tlsf_storage_report_bin* bin_report = &report.free_bins[bin_index];
        bin_report->bin_size = map_bin_to_size(bin_index);
        bin_report->block_count = 0;

        while (block_index != INVALID_BLOCK) {
            assert(0 < block_index && block_index < self.metadata_capacity);
            block_index = tlsf_blocks(&self)[block_index].next_free;
            bin_report->block_count++;
        }
    }
    return report;
}

// FIXME: I should enforce some kind of alignment for buffer, otherwise I need to add a few more
// align_forward_ptr in the arena's implementation. more specifically for the first allocation
// within a chunk in debug mode. alignof(arena_debug_header) wants buffer to be at least as much.
tlsf_index tlsf_claim_external_block(tlsf_t* self, void* buffer, usize size) {
    assert(self != NULL);
    assert(buffer != NULL);
    assert(size <= UINT32_MAX);

    tlsf_index block_index = claim_block_index(self);
    tlsf_blocks(self)[block_index] = (tlsf_block) {
        .ptr   = buffer,
        .size  = (u32)size,
        .kind  = TLSF_BLOCK_EXTERNAL,
        .flags = TLSF_BLOCK_ALLOCATED | TLSF_BLOCK_CLAIMED,
    };
    return block_index;
}

void tlsf_unclaim_external_block(tlsf_t* self, tlsf_index block_index) {
    assert(self != NULL);
    assert(block_index != INVALID_BLOCK);
    assert(block_index < self->metadata_capacity);

    unclaim_block_index(self, block_index);
    tlsf_blocks(self)[block_index] = (tlsf_block) { .kind = TLSF_BLOCK_SPARE };
}

usize tlsf_get_policy_suggested_chunk_size(const tlsf_t* self, usize min_size, usize desired_size) {
    assert(self != NULL);
    assert(min_size <= UINT32_MAX);
    assert(desired_size <= UINT32_MAX);

    const u16 max_top_index = (u16)(31 - __builtin_clz(self->top_bins));
    const u16 max_bottom_index = (u16)(15 - __builtin_clz(self->bottom_bins[max_top_index]));
    const u16 max_bin_index = (u16)(max_top_index << 4) + max_bottom_index;

    const f32 utilization = (f32)(self->backing_size - self->free_size) / (f32)self->backing_size;
    const u32 max_bin_size = map_bin_to_size(max_bin_index);

    if (desired_size > max_bin_size && utilization < self->min_utilization) {
        desired_size = max_bin_size;
    }
    min_size = round_up_block_size(min_size);
    return max_usize(min_size, desired_size);
}

bool tlsf_init(tlsf_t* self, usize max_allocations, usize backing_capacity, f32 min_utilization) {
    // NOTE: If backing_capacity > UINT32_MAX, then I could instead just insert multiple backing
    // regions, until they sum up to the requested capacity. But I don't think its that common to
    // want to reserve 4GB+ at startup, per thread potentially, so the effort doesn't seem worth it.
    assert(self != NULL);
    assert(backing_capacity < UINT32_MAX);
    assert(max_allocations < TLSF_INDEX_MAX);

    // PERF: Align to page boundary so the smallest 16/32 bin head indices are always hot
    static_assert(alignof(tlsf_index) < alignof(cacheline_t), "");
    static_assert(alignof(tlsf_block) < alignof(cacheline_t), "");

    usize metadata_size = tlsf_metadata_buffer_size(max_allocations);
    u8* metadata_buffer = aligned_alloc(alignof(cacheline_t), metadata_size);
    if (metadata_buffer == NULL) return false;

    *self = (tlsf_t) {
        .min_utilization   = min_utilization,
        .metadata_buffer   = metadata_buffer,
        .metadata_capacity = (u32)max_allocations,
        .block_pool_len    = (u32)max_allocations,
        .next_region_size  = (u32)round_up_block_size(backing_capacity),
    };
    for (u32 i = 0; i < self->metadata_capacity; ++i) {
        tlsf_block_pool(self)[i] = (tlsf_index)(self->metadata_capacity - i - 1);
    }
    return true;
}

bool tlsf_insert_backing_region(tlsf_t* self, void* buffer, usize size) {
    assert(self != NULL);
    assert(buffer != NULL);
    assert(size <= UINT32_MAX);
    assert(size % MIN_BLOCK_SIZE == 0);
    assert(is_aligned_ptr(buffer, BLOCK_ALIGNMENT));

    if (self->block_pool_len == 0) return false; // Out of metadata memory

    insert_free_block(self, buffer, (u32)size, TLSF_BLOCK_FIXED, INVALID_BLOCK, INVALID_BLOCK);
    self->backing_size += size;
    self->free_size += size;
    return true;
}

bool tlsf_destroy(tlsf_t* self, bool log_leaks) {
    assert(self != NULL);

    u64 leaked_size = 0;
    u32 leaked_count = 0;
    u32 claimed_count = self->metadata_capacity - self->block_pool_len;
    assert(claimed_count >= self->backing_regions_len);

    if (claimed_count > self->backing_regions_len) {
        for (tlsf_index block_index = 0; block_index < self->metadata_capacity; ++block_index) {
            tlsf_block* block = &tlsf_blocks(self)[block_index];
            assert(!(block->flags & TLSF_BLOCK_CLAIMED) == !(block->flags & TLSF_BLOCK_ALLOCATED));

            if (block->flags & TLSF_BLOCK_ALLOCATED) {
                leaked_size += block->size;
                leaked_count++;
            }
        }

        if (log_leaks && leaked_count > 0) {
            static const cstring suffixes[6] = {"B", "KB", "MB", "GB", "TB", "PB"};
            f64 size = (f64)leaked_size;
            u32 i = 0;
            while (size >= 1024 && i < array_len(suffixes) - 1) {
                size /= 1024;
                i++;
            }
            log_error("Leaked %.2f %s across %u allocations", size, suffixes[i], leaked_count);
        }
    }

    for (u32 i = 0; i < self->backing_regions_len; ++i) {
        free(self->backing_regions[i]);
    }
    free(self->metadata_buffer);
    memset_destroyed(self, sizeof(tlsf_t));
    return (leaked_count > 0);
}

bool tlsf_alloc_block(tlsf_t* self, u32 size, tlsf_allocation* allocation) {
    assert(self != NULL);
    assert(allocation != NULL);
    assert(size % MIN_BLOCK_SIZE == 0);

    *allocation = (tlsf_allocation) { 0 };
    if (self->block_pool_len < 2) return false; // Out of metadata memory

    u16 bin_index = map_size_to_bin((u32)size, true);
    u16 top_index = bin_index >> 4;
    u16 bottom_index = bin_index & 15;

    // Try to get a bin which is as big or bigger than the requested size.
    if (!(bottom_index = find_first_set_after(self->bottom_bins[top_index], bottom_index))) {

        // The bottom level cannot supply it, find the next smallest top level.
        // If there are no bins large enough, try allocate another backing region of memory.
        if (!(top_index = find_first_set_after(self->top_bins, top_index + 1))) {
            if (!reserve_new_backing_region(self)) {
                return false; // Out of backing memory
            }
            top_index = find_first_set_after(self->top_bins, top_index + 1);
            assert(top_index != 0);
        }
        // Got a resident top level, find the smallest bottom level bin inside that.
        bottom_index = (u16)__builtin_ctz(self->bottom_bins[top_index]);
    }
    bin_index = (u16)(top_index << 4) + bottom_index;

    const tlsf_index block_index = tlsf_bin_heads(self)[bin_index];
    tlsf_block* block = &tlsf_blocks(self)[block_index];

    assert(block->kind != TLSF_BLOCK_SPARE);
    assert(block->kind != TLSF_BLOCK_EXTERNAL);
    assert(block->flags & TLSF_BLOCK_HEAD);
    assert(block->flags & TLSF_BLOCK_CLAIMED);
    assert((block->flags & TLSF_BLOCK_ALLOCATED) == 0);

    tlsf_bin_heads(self)[bin_index] = block->next_free;
    self->free_size -= block->size;

    if (block->next_free != INVALID_BLOCK) {
        tlsf_block* new_head = &tlsf_blocks(self)[block->next_free];
        new_head->prev_free = bin_index;
        new_head->flags |= TLSF_BLOCK_HEAD;
    } else {
        // The bottom bin is now empty, zero its corresponding bit
        self->bottom_bins[top_index] &= ~(1u << bottom_index);

        // If all bottom bins are now empty, zero the corresponding top bit
        if (self->bottom_bins[top_index] == 0) {
            self->top_bins &= ~(1u << top_index);
        }
    }

    const u32 remaining_size = block->size - size;
    if (remaining_size > 0) {
        insert_free_block(self, block->ptr + size, remaining_size, block->kind, block_index, block->next_phys);
    }

    *block = (tlsf_block) {
        .ptr       = block->ptr,
        .size      = size,
        .kind      = block->kind,
        .flags     = TLSF_BLOCK_ALLOCATED | TLSF_BLOCK_CLAIMED,
        .next_phys = block->next_phys,
        .prev_phys = block->prev_phys,
    };
    *allocation = (tlsf_allocation) {
        .size        = size,
        .ptr         = block->ptr,
        .block_index = block_index,
    };
    return true;
}

bool tlsf_resize_block(tlsf_t* self, tlsf_index block_index, u32 new_size) {
    assert(self != NULL);
    assert(new_size % MIN_BLOCK_SIZE == 0);
    assert(block_index != INVALID_BLOCK);
    assert(block_index < self->metadata_capacity);

    tlsf_block* block = &tlsf_blocks(self)[block_index];
    if (block->next_phys == INVALID_BLOCK) return false;
    if (block->kind == TLSF_BLOCK_EXTERNAL) return false;

    assert(block->kind != TLSF_BLOCK_SPARE);
    assert(block->flags & TLSF_BLOCK_CLAIMED);
    assert(block->flags & TLSF_BLOCK_ALLOCATED);
    assert((block->flags & TLSF_BLOCK_HEAD) == 0);

    // All free nodes are already as coalesced as they could be, if our next physical neighbour is
    // free but not large enough to grow into then it is simply impossible to resize.
    tlsf_block* next_block = &tlsf_blocks(self)[block->next_phys];
    if (next_block->flags & TLSF_BLOCK_ALLOCATED) return false;
    if (block->size + next_block->size < new_size) return false;

    // The same block will be immediately reclaimed by `insert_free_block`.
    const u32 remaining_size = block->size + next_block->size - new_size;
    remove_free_block(self, block->next_phys);

    if (remaining_size > 0) {
        insert_free_block(self, block->ptr + new_size, remaining_size, block->kind, block_index, next_block->next_phys);
    }
    block->size = new_size;
    return true;
}

void tlsf_free_block(tlsf_t* self, tlsf_index block_index) {
    assert(self != NULL);
    assert(block_index != INVALID_BLOCK);
    assert(block_index < self->metadata_capacity);

    tlsf_block* block = &tlsf_blocks(self)[block_index];
    if (block->kind == TLSF_BLOCK_EXTERNAL) {
        tlsf_unclaim_external_block(self, block_index);
        return;
    }
    assert(block->kind != TLSF_BLOCK_SPARE);
    assert(block->flags & TLSF_BLOCK_CLAIMED);
    assert(block->flags & TLSF_BLOCK_ALLOCATED);
    assert((block->flags & TLSF_BLOCK_HEAD) == 0);

    if (block->prev_phys != INVALID_BLOCK && is_block_free(self, block->prev_phys)) {
        const tlsf_block prev_block = tlsf_blocks(self)[block->prev_phys];
        block->ptr = prev_block.ptr;
        block->size += prev_block.size;
        remove_free_block(self, block->prev_phys);
        block->prev_phys = prev_block.prev_phys;
    }

    if (block->next_phys != INVALID_BLOCK && is_block_free(self, block->next_phys)) {
        const tlsf_block next_block = tlsf_blocks(self)[block->next_phys];
        block->size += next_block.size;
        remove_free_block(self, block->next_phys);
        block->next_phys = next_block.next_phys;
    }

    // The same block will be immediately reclaimed by `insert_free_block`.
    unclaim_block_index(self, block_index);
    insert_free_block(self, block->ptr, block->size, block->kind, block->prev_phys, block->next_phys);
}
