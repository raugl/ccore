#include "core/allocator.h"
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
