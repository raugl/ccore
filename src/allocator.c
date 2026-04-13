#include "allocator.h"

// =================================================================================================
// Section: Arena Allocator
// =================================================================================================
arena_t arena_init_fixed(void* buffer, u32 size) {
    assert(buffer != NULL);
    // assert(false); // TODO: alignment of max_align_t

    return (arena_t) {
        .ptr = buffer,
        .capacity = size,
        .kind = ARENA_BACKING_FIXED,
    };
}

arena_t arena_init_alloc(allocator_t* alloc, u32 size) {
    assert(alloc != NULL);

    // TODO: ptr alignment of max_align_t
    u8* ptr = alloc_alloc(alloc, u8, size);
    return (arena_t) {
        .ptr = ptr,
        .alloc = alloc,
        .capacity = size,
        .kind = ARENA_BACKING_ALLOC,
    };
}

// TODO: Allocate virtual memory, respect max_align_t
arena_t arena_init_virtual(u32 size) {
    return (arena_t) {
        .ptr = NULL,
        .capacity = size,
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
        alloc_free(self->alloc, *self);
        break;
    case ARENA_BACKING_VIRTUAL:
        panic("TODO: unimplemented");
        break;
    }
    *self = (arena_t) {};
}

void arena_clear(arena_t* self) {
    self->index = 0;
}

void* arena_alloc_impl(arena_t* self, u32 size) {
    assert(self != NULL);
    assert(self->index + size <= self->capacity);

    // Assumes that the original buffer and it's internal tail index are always aligned to `max_align_t`
    void* start = self->ptr + self->index;
    self->index = align_forward(self->index + size, alignof(max_align_t));
    return start;
}

void* arena_realloc_impl(arena_t* self, void* ptr, u32 old_size, u32 new_size) {
    u8* start = ptr;
    assert(self != NULL);
    assert(self->ptr <= start);
    assert(start + old_size <= self->ptr + self->index);

    if (start + old_size == self->ptr + self->index) {
        self->index += (isize)new_size - (isize)old_size;
        assert(self->index <= self->capacity);
        return start;
    }
    start = arena_alloc_impl(self, new_size);
    memcpy(start, ptr, old_size);
    return start;
}
