// A fast, hard real-time O(1) heap allocator. It's intended to be used as a fast userspace page
// allocator instead of just as a replacement for `malloc`. Its minimum allocation size is 256 bytes,
// and with the same alignment. It supports larger allocation, but they must be multiples of 256 too.
// It attempts to geometrically grow its backing storage on demand through the OS allocator.
// Currently that's just malloc, but eventually it should use mmap/wasm.memory/VirtualAlloc.
//
// Its intended to only ever back arena allocators, and there is _some_ coupling between the two.
// Because of it the creation and destruction of a new throwaway scratch arena with a default size
// (256-8192) should almost be fast enough to never have to thread a scratch allocator through the
// call stack ever again.

#pragma once
#include "common.h"

#define TLSF_NIL_INDEX       0
#define TLSF_INDEX_MAX       ((1u << bit_sizeof(tlsf_index)) - 1)

#define TLSF_TOP_LEVELS      21
#define TLSF_BOTTOM_LEVELS   16
#define TLSF_BIN_COUNT       (TLSF_TOP_LEVELS * TLSF_BOTTOM_LEVELS - 1)

#define TLSF_MIN_BLOCK_SIZE  256
#define TLSF_BLOCK_ALIGNMENT 256

typedef enum PACKED tlsf_block_kind {
    TLSF_BLOCK_UNCLAIMED = 0, // Metadata slot is available; no backing memory is associated with it.
    TLSF_BLOCK_MANAGED   = 1, // Backing memory is owned by TLSF and managed normally.
    TLSF_BLOCK_FIXED     = 2, // Backing memory is caller-owned; TLSF may manage the block but must not free it.
    TLSF_BLOCK_EXTERNAL  = 3, // Represents caller-managed memory that TLSF cannot split, coalesce, or free.
} tlsf_block_kind;

typedef enum PACKED tlsf_block_flags {
    TLSF_BLOCK_HEAD      = 1, // Block is the first entry in its free-list bin.
    TLSF_BLOCK_ALLOCATED = 2, // Block's backing memory is currently allocated to the caller.
} tlsf_block_flags;

typedef u16 tlsf_index;

typedef struct tlsf_block {
    u8*              ptr;
    u32              size;
    tlsf_block_kind  kind;
    tlsf_block_flags flags;
    union {                      // Tagged by flags & TLSF_BLOCK_HEAD, both 0/INVALID_INDEX when flags & TLSF_BLOCK_ALLOCATED
        u16          bin_index;  // Index to the bin's freelist in which this *free* block recedes
        tlsf_index   prev_free;  // The link to the previous neighbour when inside a freelist
    };
    union {                      // Tagged by flags & TLSF_BLOCK_ALLOCATED
        tlsf_index   next_free;  // The link to the next neighbour when inside a freelist, when not allocated
        tlsf_index   prev_chunk; // The link to the previous chunk inside a arena, when allocated
    };
    tlsf_index       prev_phys, next_phys;
    // u16 _padding;
} tlsf_block;

typedef struct tlsf_t {
    u64 free_size;
    u64 backing_size;
    u8* metadata_buffer;
    u32 metadata_capacity;
    u32 unclaimed_blocks;
    f32 min_utilization;
    u32 top_bins;
    u16 bottom_bins[TLSF_TOP_LEVELS];
    u32 next_region_size;
    u32 backing_regions_len;
    u8* backing_regions[32];
} tlsf_t;

extern thread_local tlsf_t* _ccore_global_tlsf;

typedef struct tlsf_allocation {
    void*      ptr;
    u32        size;
    tlsf_index block_index;
} tlsf_allocation;

typedef struct tlsf_storage_report {
    u64 free_size; // This is accumulated from all the distinct backing regions, this is why it could be larger than UINT32_MAX
    u32 largest_free_block;
} tlsf_storage_report;

typedef struct tlsf_storage_report_bin {
    u64 bin_size;
    u32 block_count;
} tlsf_storage_report_bin;

typedef struct tlsf_storage_report_full {
    tlsf_storage_report_bin free_bins[TLSF_BIN_COUNT];
} tlsf_storage_report_full;

// TODO: Add some function to manually request shrinking/freeing of the backing regions.
bool tlsf_init(tlsf_t* self, usize max_allocations, usize initial_capacity, f32 min_utilization);
void tlsf_insert_fixed_backing_region(tlsf_t* self, void* buffer, usize size);
bool tlsf_destroy(tlsf_t* self, bool log_leaks);

tlsf_storage_report tlsf_get_storage_report(const tlsf_t* self);
tlsf_storage_report_full tlsf_get_storage_report_full(const tlsf_t* self);
u32 tlsf_get_policy_suggested_chunk_size(const tlsf_t* self, usize min_size, usize desired_size);
tlsf_index tlsf_claim_external_block(tlsf_t* self, void* buffer, usize size);
void tlsf_unclaim_external_block(tlsf_t* self, tlsf_index block_index);

bool tlsf_alloc_block(tlsf_t* self, u32 size, tlsf_allocation* allocation);
bool tlsf_resize_block(tlsf_t* self, tlsf_index block_index, u32 new_size);
void tlsf_free_block(tlsf_t* self, tlsf_index block_index);

// TLSF metadata buffer layout:
// [bin_heads][block_pool][alignment padding][blocks]
//
// bin_heads:  alignas(cacheline_t) tlsf_index[TLSF_BIN_COUNT]
// block_pool: tlsf_index[metadata_capacity]
// blocks:     tlsf_block[metadata_capacity]

#define tlsf_bin_heads(self)                                                                        \
    ((tlsf_index*)(self)->metadata_buffer)

#define tlsf_unclaimed_pool(self)                                                                   \
    ((tlsf_index*)((self)->metadata_buffer + sizeof(tlsf_index) * TLSF_BIN_COUNT))

#define tlsf_blocks(self)                                                                           \
    ((tlsf_block*)align_forward_ptr(tlsf_unclaimed_pool(self) + (self)->metadata_capacity, alignof(tlsf_block)))

#define tlsf_metadata_buffer_size(max_allocations)                                                  \
    (align_forward(sizeof(tlsf_index) * (TLSF_BIN_COUNT + (max_allocations)), alignof(tlsf_block)) + sizeof(tlsf_block) * (max_allocations))
