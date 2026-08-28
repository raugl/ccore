#define GENERICS_IMPLEMENTATION
#include "core/allocator.h"
#include "core/debug_allocator.h"
#include "core/math.h"

static void* null_allocator_proc(allocator_operation op, void* self, void* old_ptr, usize old_size, usize new_size, usize align) {
    (void)op, (void)self, (void)old_ptr, (void)old_size, (void)new_size, (void)align;
    return NULL;
}

allocator_t allocator_init_null(void) {
    return (allocator_t) { .proc = &null_allocator_proc };
}

static void* malloc_allocator_proc(allocator_operation op, void* self, void* old_ptr, usize old_size, usize new_size, usize align) {
    (void)self, (void)old_size;
    switch (op) {
    case ALLOCATOR_OPERATION_ALLOC:
        return aligned_alloc(align, new_size);
    case ALLOCATOR_OPERATION_REALLOC:
        return realloc(old_ptr, new_size);
    case ALLOCATOR_OPERATION_RESIZE:
        return NULL;
    case ALLOCATOR_OPERATION_FREE:
        free(old_ptr);
        return NULL;
    }
}

allocator_t allocator_init_malloc(void) {
    return (allocator_t) { .proc = &malloc_allocator_proc };
}

static void* arena_allocator_proc(allocator_operation op, void* self, void* old_ptr, usize old_size, usize new_size, usize align) {
    switch (op) {
    case ALLOCATOR_OPERATION_ALLOC:
        return arena_alloc_raw(self, new_size, align);
    case ALLOCATOR_OPERATION_REALLOC:
        return arena_realloc_raw(self, old_ptr, old_size, new_size, align);
    case ALLOCATOR_OPERATION_RESIZE:
        return arena_resize_raw(self, old_ptr, old_size, new_size) ? (void*)sizeof(max_align_t) : NULL;
    case ALLOCATOR_OPERATION_FREE:
        arena_free_raw(self, old_ptr, old_size);
        return NULL;
    }
}

allocator_t allocator_init_arena(arena_t* arena) {
    return (allocator_t) { .self = arena, .proc = &arena_allocator_proc };
}

arena_t arena_init_fixed(void* buffer, usize size) {
    assert(size == 0 || buffer != NULL);
    return (arena_t) {
        .ptr = buffer,
        .capacity = (u32)size,
        .allocator = allocator_init_null(),
    };
}

arena_t arena_init_alloc(allocator_t allocator, usize size) {
    assert(allocator.proc != NULL);
    u8* ptr = mem_alloc(allocator, u8, size);
    assert(ptr != NULL); // TODO: I know this is bad, I'll fix it later. It needs to return the allocation failure

    return (arena_t) {
        .ptr = ptr,
        .capacity = (u32)size,
        .allocator = allocator,
    };
}

void arena_release(arena_t* self) {
    assert(self != NULL);
    mem_free(self->allocator, self->ptr, self->capacity);
    memset(self, 0xDE, sizeof(arena_t));
}

void arena_clear(arena_t* self) {
    assert(self != NULL);
    log_trace("arena_clear: (%uB total)", self->offset);
    memset(self->ptr, 0xDE, self->offset);
    self->offset = 0;
}

arena_t arena_checkpoint(arena_t* self) {
    return *self;
}

void arena_rollback(arena_t* self, arena_t checkpoint) {
    assert(self != NULL);
    assert(self->ptr == checkpoint.ptr);
    assert(self->capacity == checkpoint.capacity);
    assert(self->allocator.self == checkpoint.allocator.self);
    assert(self->allocator.proc == checkpoint.allocator.proc);

    log_trace(
        "arena_rollback: %uB (%uB total)",
        self->offset - checkpoint.offset,
        checkpoint.offset
    );
    *self = checkpoint;
}

void* arena_alloc_raw(arena_t* self, usize size, usize align) {
    assert(self != NULL);
    assert(is_aligned(size, align));

    log_trace("arena_alloc: %zuB (%uB total)", size, self->offset);
    u8* ptr = align_forward_ptr(self->ptr + self->offset, align);
    if (ptr + size > self->ptr + self->capacity) return NULL;

    self->offset = (u32)((ptr + size) - self->ptr);
    memset(ptr, 0, size);
    return ptr;
}

bool arena_resize_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size) {
    assert(old_ptr != NULL);
    assert(self != NULL);
    assert((u8*)old_ptr >= self->ptr);
    assert((u8*)old_ptr + old_size <= self->ptr + self->offset);

    bool success = (u8*)old_ptr + old_size < self->ptr + self->offset
        && (u8*)old_ptr + new_size > self->ptr + self->capacity;

    log_trace(
        "arena_resize: %zuB -> %zuB (%uB total)%s",
        old_size,
        new_size,
        self->offset,
        success ? "" : " (failed)"
    );
    if (!success) return false;

    // if ((u8*)ptr + old_size < self->ptr + self->offset) return false;
    // if ((u8*)ptr + new_size > self->ptr + self->capacity) return false;

    if (new_size > old_size) {
        self->offset += (u32)(new_size - old_size);
        memset((u8*)old_ptr + old_size, 0, new_size - old_size);
    } else {
        self->offset -= (u32)(old_size - new_size);
        memset((u8*)old_ptr + new_size, 0xDE, old_size - new_size);
    }
    return true;
}

void* arena_realloc_raw(arena_t* self, void* old_ptr, usize old_size, usize new_size, usize align) {
    assert(self != NULL);
    assert(is_aligned_ptr(old_ptr, align));
    assert(is_aligned(old_size, align));
    assert(is_aligned(new_size, align));

    log_trace("arena_realloc: %zuB -> %zuB (%uB total)", old_size, new_size, self->offset);
    if (old_ptr != NULL && arena_resize_raw(self, old_ptr, old_size, new_size)) {
        return old_ptr;
    }
    u8* new_ptr = arena_alloc_raw(self, new_size, align);
    if (new_size > old_size) {
        memcpy(new_ptr, old_ptr, old_size);
        memset(new_ptr + old_size, 0, new_size - old_size);
    } else {
        memcpy(new_ptr, old_ptr, new_size);
    }
    arena_free_raw(self, old_ptr, old_size);
    return new_ptr;
}

void arena_free_raw(arena_t* self, void* old_ptr, usize old_size) {
    if (old_ptr != NULL) {
        log_trace("arena_free: %zuB (%uB total)", old_size, self->offset);
        arena_resize_raw(self, old_ptr, old_size, 0);
        memset(old_ptr, 0xDE, old_size);
    }
}

#define CANARY_SIZE 16

// `size` is assumed to be the user requested size, `ptr` should include an extra `CANARY_SIZE * 2`
static u32 check_canaries(u8* ptr, usize size) {
    u32 result = 0;
    for (i32 i = -CANARY_SIZE; i < 0; ++i) {
        if (ptr[i] != 0xAA) {
            result += 1;
            break;
        }
    }
    for (u32 i = 0; i < CANARY_SIZE; ++i) {
        if (ptr[size + i] != 0xAA) {
            result += 2;
            break;
        }
    }
    return result;
}

// TODO: Pass caller location information into here
static void assert_allocation(allocation_info* info, allocator_operation operation, void* ptr, usize size) {
    static const cstring op_names[4] = { "alloc", "realloc", "resize", "free" };
    static const cstring bounds[4] = { "", "front", "back", "front and the back" };

    const cstring op = op_names[operation];
    if (!info)              panic("%s of untracked pointer %p", op, ptr);
    if (!info->alive)       panic("%s of freed pointer %p", op, ptr);
    if (info->size != size) panic("%s size mismatch for %p: allocated %zu, passed as %zu", op, ptr, info->size, size);

    const u32 side = check_canaries(ptr, size);
    if (side != 0)          panic("overran the %s bounds of %p (was %zu bytes)", bounds[side], ptr, info->size);
}

// FIXME: I don't take into accout the user's requested alignment. I always just offset it by 16 and
// hope for the best. I should use the currently useless `align` field from `allocation_info` to fix
// this.
static void* debug_allocator_proc(allocator_operation op, void* self_, void* old_ptr, usize old_size, usize new_size, usize align) {
    debug_allocator* self = self_;
    const allocation_info new_info = {
        .size  = new_size,
        .align = (u32)align,
        .alive = true,
    };

    switch (op) {
    case ALLOCATOR_OPERATION_ALLOC: {
        if (self->fail_after_n == 0) return NULL;
        if (self->fail_after_n > 0) self->fail_after_n--;

        if (!map_alloc_info_ensure_capacity(&self->_allocations, self->_allocations.len + 1)) {
            log_error("debug_allocator metadata allocation failed");
            return NULL;
        }

        new_size += CANARY_SIZE * 2;
        u8* ptr = self->_backing.proc(op, self->_backing.self, 0, 0, new_size, align);
        if (!ptr) return NULL;

        memset_undefined(ptr, new_size);
        assert(map_alloc_info_put(&self->_allocations, ptr, new_info));
        return ptr + CANARY_SIZE;
    }
    case ALLOCATOR_OPERATION_REALLOC: {
        allocation_info* info = old_ptr ? map_alloc_info_get(&self->_allocations, old_ptr) : NULL;
        if (old_ptr) assert_allocation(info, op, old_ptr, old_size);
        if (self->fail_after_n == 0) return NULL;
        if (self->fail_after_n > 0) self->fail_after_n--;

        if (!map_alloc_info_ensure_capacity(&self->_allocations, self->_allocations.len + 1)) {
            log_error("debug_allocator metadata allocation failed");
            return NULL;
        }

        old_size += CANARY_SIZE * 2;
        new_size += CANARY_SIZE * 2;
        old_ptr  = (u8*)old_ptr - CANARY_SIZE;
        u8* ptr = self->_backing.proc(op, self->_backing.self, old_ptr, old_size, new_size, align);
        if (!ptr) return NULL;

        if (new_size > old_size)  memset_undefined(ptr + old_size, new_size - old_size);
        if (new_size <= old_size) memset_undefined(ptr + new_size - CANARY_SIZE, CANARY_SIZE);
        if (old_ptr) info->alive = (old_ptr == ptr);

        // FIXME: I'd want to also set the freed regions to `0xDE`, but its basically impossible
        // for libc's `realloc`, and even for my custom allocators I should still request them
        // to poison the memory instead of me trying to do it after its been already released.

        assert(map_alloc_info_put(&self->_allocations, ptr, new_info));
        return ptr + CANARY_SIZE;
    }
    case ALLOCATOR_OPERATION_RESIZE: {
        allocation_info* info = map_alloc_info_get(&self->_allocations, old_ptr);
        assert_allocation(info, op, old_ptr, old_size);
        if (self->force_resize_fail) return NULL;

        old_size += CANARY_SIZE * 2;
        new_size += CANARY_SIZE * 2;
        old_ptr  = (u8*)old_ptr - CANARY_SIZE;
        u8* ptr = self->_backing.proc(op, self->_backing.self, old_ptr, old_size, new_size, 0);
        if (!ptr) return NULL;

        if (new_size > old_size)  memset_undefined(ptr + old_size, new_size - old_size);
        if (new_size <= old_size) memset_undefined(ptr + new_size - CANARY_SIZE, CANARY_SIZE);
        info->size = new_size;

        // FIXME: I'd want to also set the freed regions to `0xDE`, but its basically impossible
        // for libc's `realloc`, and even for my custom allocators I should still request them
        // to poison the memory instead of me trying to do it after its been already released.

        return ptr;
    }
    case ALLOCATOR_OPERATION_FREE: {
        if (old_ptr == NULL) return NULL;
        allocation_info* info = map_alloc_info_get(&self->_allocations, old_ptr);
        assert_allocation(info, op, old_ptr, old_size);

        old_size += CANARY_SIZE * 2;
        old_ptr  = (u8*)old_ptr - CANARY_SIZE;
        memset_destroyed(old_ptr, info->size);

        self->_backing.proc(op, self->_backing.self, old_ptr, old_size, 0, 0);
        info->alive = false;
        return NULL;
    }
    }
}

allocator_t allocator_init_debug(debug_allocator* debug) {
    return (allocator_t) { .self = debug, .proc = &debug_allocator_proc };
}

debug_allocator debug_allocator_init(allocator_t internal, allocator_t backing) {
    return (debug_allocator) {
        ._backing     = backing,
        ._allocations = map_alloc_info_init_alloc(internal),
        .fail_after_n = -1,
    };
}

bool debug_allocator_release(debug_allocator* debug, bool log_leaks) {
    assert(debug != NULL);

    u32 leaked_count = 0;
    usize leaked_size = 0;
    map_iter_t iter = map_alloc_info_iter(debug->_allocations);

    while (map_iter_next(&iter)) {
        allocation_info info = *(allocation_info*)iter.value;
        if (info.alive) {
            leaked_size += info.size;
            leaked_count += 1;
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
        log_error("Leaked %.2f %s across %d allocations", size, suffixes[i], leaked_count);
    }

    map_alloc_info_release(&debug->_allocations);
    memset_destroyed(debug, sizeof(debug_allocator));
    return (leaked_count > 0);
}

HASHMAP_IMPL_RAW(void*, allocation_info, map_alloc_info, hash_ptr, equal_bytes)
