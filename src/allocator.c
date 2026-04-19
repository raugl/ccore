#include "allocator.h"
#include <stdlib.h>
#include "math.h"

void* alloc_alloc_raw(allocator_t* self, usize size) {
    void* mem = malloc(size);
    if (mem == NULL) panic("malloc out of memory");
    return mem;
}

void* alloc_realloc_raw(allocator_t* self, void* ptr, usize old_size, usize new_size) {
    void* mem = realloc(ptr, new_size);
    if (mem == NULL) panic("realloc out of memory");
    return mem;
}

void alloc_free_raw(allocator_t* self, void* ptr, usize size) {
    free(ptr);
}

// =================================================================================================
// Section: Arena Allocator
// =================================================================================================
arena_t arena_init_fixed(void* buffer, usize size) {
    assert(buffer != NULL);
    assert((uintptr_t)buffer % sizeof(max_align_t) == 0);

    return (arena_t) {
        .ptr = buffer,
        .capacity = size,
        .kind = ARENA_BACKING_FIXED,
    };
}

arena_t arena_init_alloc(allocator_t* alloc, usize size) {
    assert(alloc != NULL);

    u8* ptr = alloc_alloc(alloc, u8, size);
    return (arena_t) {
        .ptr = ptr,
        .alloc = alloc,
        .capacity = size,
        .kind = ARENA_BACKING_ALLOC,
    };
}

// TODO: Allocate virtual memory, respect max_align_t
arena_t arena_init_virtual(usize size) {
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

void* arena_alloc_raw(arena_t* self, usize size) {
    assert(self != NULL);
    assert(self->index + size <= self->capacity);

    // Assumes that the original buffer and it's internal tail index are always aligned to `max_align_t`
    void* start = self->ptr + self->index;
    self->index = align_forward(self->index + size, alignof(max_align_t));
    return start;
}

// TODO: return null if grow in place fails
void* arena_realloc_raw(arena_t* self, void* ptr, usize old_size, usize new_size) {
    u8* start = ptr;
    assert(self != NULL);
    assert(self->ptr <= start);
    assert(start + old_size <= self->ptr + self->index);

    if (start + old_size == self->ptr + self->index) {
        self->index += (isize)new_size - (isize)old_size;
        assert(self->index <= self->capacity);
        return start;
    }
    panic("arena should only try to grow in place, not memcpy into a new allocation");
    return NULL;

    start = arena_alloc_raw(self, new_size);
    memcpy(start, ptr, old_size);
    return start;
}
