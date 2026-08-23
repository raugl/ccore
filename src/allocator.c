#include "core/allocator.h"
#include "core/math.h"

static void* null_allocator_alloc(void* ctx, usize size, usize align) {
    (void)ctx, (void)size, (void)align;
    return NULL;
}

static bool null_allocator_resize(void* ctx, void* ptr, usize old_size, usize new_size) {
    (void)ctx, (void)ptr, (void)old_size, (void)new_size;
    return false;
}

static void*
null_allocator_realloc(void* ctx, void* ptr, usize old_size, usize new_size, usize align) {
    (void)ctx, (void)ptr, (void)old_size, (void)new_size, (void)align;
    return NULL;
}

static void null_allocator_free(void* ctx, void* ptr, usize size) {
    (void)ctx, (void)ptr, (void)size;
}

static const allocator_vtable_t null_allocator_vtable = {
    .alloc = &null_allocator_alloc,
    .resize = &null_allocator_resize,
    .realloc = &null_allocator_realloc,
    .free = &null_allocator_free,
};

static void* malloc_allocator_alloc(void* ctx, usize size, usize align) {
    (void)ctx;
    return aligned_alloc(align, size);
}

static bool malloc_allocator_resize(void* ctx, void* ptr, usize old_size, usize new_size) {
    (void)ctx, (void)ptr, (void)old_size, (void)new_size;
    return false;
}

static void*
malloc_allocator_realloc(void* ctx, void* ptr, usize old_size, usize new_size, usize align) {
    (void)ctx, (void)old_size, (void)align;
    return realloc(ptr, new_size);
}

static void malloc_allocator_free(void* ctx, void* ptr, usize size) {
    (void)ctx, (void)size;
    free(ptr);
}

static const allocator_vtable_t malloc_allocator_vtable = {
    .alloc = &malloc_allocator_alloc,
    .resize = &malloc_allocator_resize,
    .realloc = &malloc_allocator_realloc,
    .free = &malloc_allocator_free,
};

static void* arena_allocator_alloc(void* ctx, usize size, usize align) {
    return arena_alloc_raw(ctx, size, align);
}

static bool arena_allocator_resize(void* ctx, void* ptr, usize old_size, usize new_size) {
    return arena_resize_raw(ctx, ptr, old_size, new_size);
}

static void*
arena_allocator_realloc(void* ctx, void* ptr, usize old_size, usize new_size, usize align) {
    return arena_realloc_raw(ctx, ptr, old_size, new_size, align);
}

static void arena_allocator_free(void* ctx, void* ptr, usize size) {
    arena_free_raw(ctx, ptr, size);
}

static const allocator_vtable_t arena_allocator_vtable = {
    .alloc = &arena_allocator_alloc,
    .resize = &arena_allocator_resize,
    .realloc = &arena_allocator_realloc,
    .free = &arena_allocator_free,
};

allocator_t allocator_init_null(void) {
    return (allocator_t) { .vtable = &null_allocator_vtable };
}

allocator_t allocator_init_malloc(void) {
    return (allocator_t) { .vtable = &malloc_allocator_vtable };
}

allocator_t allocator_init_arena(arena_t* arena) {
    return (allocator_t) { .ctx = arena, .vtable = &arena_allocator_vtable };
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
    assert(allocator.vtable != NULL);
    u8* ptr = mem_alloc(allocator, u8, size);
    assert(ptr != NULL); // TODO: I know this is bad, I'll fix it later

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
    assert(self->allocator.ctx == checkpoint.allocator.ctx);
    assert(self->allocator.vtable == checkpoint.allocator.vtable);

    log_trace(
        "arena_rollback: %uB (%uB total)",
        self->offset - checkpoint.offset,
        checkpoint.offset
    );
    *self = checkpoint;
}

void* arena_alloc_raw(arena_t* self, usize size, usize align) {
    assert(self != NULL);
    assert(is_aligned2(size, align));

    log_trace("arena_alloc: %zuB (%uB total)", size, self->offset);
    u8* ptr = align_forward_ptr(self->ptr + self->offset, align);
    if (ptr + size > self->ptr + self->capacity) return NULL;

    self->offset = (u32)((ptr + size) - self->ptr);
    memset(ptr, 0, size);
    return ptr;
}

bool arena_resize_raw(arena_t* self, void* ptr, usize old_size, usize new_size) {
    assert(ptr != NULL);
    assert(self != NULL);
    assert((u8*)ptr >= self->ptr);
    assert((u8*)ptr + old_size <= self->ptr + self->offset);

    bool success = (u8*)ptr + old_size < self->ptr + self->offset
        && (u8*)ptr + new_size > self->ptr + self->capacity;

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
        memset((u8*)ptr + old_size, 0, new_size - old_size);
    } else {
        self->offset -= (u32)(old_size - new_size);
        memset((u8*)ptr + new_size, 0xDE, old_size - new_size);
    }
    return true;
}

void* arena_realloc_raw(arena_t* self, void* ptr, usize old_size, usize new_size, usize align) {
    assert(self != NULL);
    assert(is_aligned2_ptr(ptr, align));
    assert(is_aligned2(old_size, align));
    assert(is_aligned2(new_size, align));

    log_trace("arena_realloc: %zuB -> %zuB (%uB total)", old_size, new_size, self->offset);
    if (ptr != NULL && arena_resize_raw(self, ptr, old_size, new_size)) {
        return ptr;
    }
    u8* new_ptr = arena_alloc_raw(self, new_size, align);
    if (new_size > old_size) {
        memcpy(new_ptr, ptr, old_size);
        memset(new_ptr + old_size, 0, new_size - old_size);
    } else {
        memcpy(new_ptr, ptr, new_size);
    }
    arena_free_raw(self, ptr, old_size);
    return new_ptr;
}

void arena_free_raw(arena_t* self, void* ptr, usize size) {
    if (ptr != NULL) {
        log_trace("arena_free: %zuB (%uB total)", size, self->offset);
        arena_resize_raw(self, ptr, size, 0);
        memset(ptr, 0xDE, size);
    }
}
