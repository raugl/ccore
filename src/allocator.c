#include "allocator.h"
#include <stdlib.h>
#include "math.h"

// TODO: General purpose interface
static void* malloc_allocator_fn(void* ctx, void* ptr, usize old_size, usize new_size) {
    (void)ctx;
    if (ptr == NULL && old_size == 0) {
        return malloc(new_size);
    }
    if (ptr != NULL && new_size > old_size) {
        return realloc(ptr, new_size);
    }
    if (ptr != NULL && new_size == 0) {
        free(ptr);
    }
    return NULL;
}

void* alloc_alloc(allocator_t* self, usize size) {
    (void)self;
    void* mem = malloc(size);
    if (mem == NULL) panic("malloc out of memory");
    return mem;
}

void* alloc_realloc(allocator_t* self, void* ptr, usize old_size, usize new_size) {
    (void)self, (void)old_size;
    void* mem = realloc(ptr, new_size);
    if (mem == NULL) panic("realloc out of memory");
    return mem;
}

void alloc_free(allocator_t* self, void* ptr, usize size) {
    (void)self, (void)size;
    free(ptr);
}

// =================================================================================================
// Section: Arena Allocator
// =================================================================================================
arena_t arena_init_fixed(void* buffer, usize size) {
    assert(buffer != NULL);
    assert((uintptr_t)buffer % sizeof(max_align_t) == 0);

    return (arena_t) {
        .ptr_ = buffer,
        .capacity = (u32)size,
        .kind = ARENA_BACKING_FIXED,
    };
}

arena_t arena_init_alloc(allocator_t* alloc, usize size) {
    assert(alloc != NULL);

    u8* ptr = alloc_alloc(alloc, size * sizeof(u8));
    return (arena_t) {
        .ptr_ = ptr,
        .alloc = alloc,
        .capacity = (u32)size,
        .kind = ARENA_BACKING_ALLOC,
    };
}

// TODO: Allocate virtual memory, respect max_align_t
arena_t arena_init_virtual(usize size) {
    return (arena_t) {
        .ptr_ = NULL,
        .capacity = (u32)size,
        .kind = ARENA_BACKING_VIRTUAL,
    };
}

void arena_release(arena_t* self) {
    assert(self != NULL);

    switch (self->kind) {
    case ARENA_BACKING_FIXED:
        log_warn("releasing a fixed buffer arena");
        break;
    case ARENA_BACKING_ALLOC:
        assert(self->alloc != NULL);
        alloc_free(self->alloc, self->ptr_, self->capacity * sizeof(u8));
        break;
    case ARENA_BACKING_VIRTUAL:
        panic("TODO: unimplemented");
        break;
    }
    memset(self, 0xDE, sizeof(arena_t));
}

void arena_clear(arena_t* self) {
    self->index = 0;
}

void* arena_alloc(arena_t* self, usize size) {
    assert(self != NULL);
    assert(self->index + size <= self->capacity);

    // Assumes that the original buffer and it's internal tail index are always aligned to
    // `max_align_t`
    void* start = self->ptr_ + self->index;
    self->index = (u32)align_forward(self->index + size, alignof(max_align_t));
    return start;
}

void* arena_realloc(arena_t* self, void* ptr, usize old_size, usize new_size) {
    u8* start = ptr;
    assert(self != NULL);
    assert(self->ptr_ <= start);
    assert(start + old_size <= self->ptr_ + self->index);

    if (start + old_size == self->ptr_ + self->index) {
        self->index += (isize)new_size - (isize)old_size;
        assert(self->index <= self->capacity);
        return start;
    }
    start = arena_alloc(self, new_size);
    memcpy(start, ptr, old_size);
    return start;
}

bool arena_try_resize(arena_t* self, void* ptr, usize old_size, usize new_size) {
    u8* start = ptr;
    assert(self != NULL);
    assert(self->ptr_ <= start);
    assert(start + old_size <= self->ptr_ + self->index);
    bool is_last = start + old_size == self->ptr_ + self->index;

    if (is_last && new_size > old_size) {
        self->index += new_size - old_size;
        assert(self->index <= self->capacity);
        return true;
    }
    if (is_last && new_size < old_size) {
        self->index -= old_size - new_size;
        return true;
    }
    return false;
}
