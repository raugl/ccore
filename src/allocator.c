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
    unreachable;
}

void* alloc_alloc(allocator_t* self, usize size) {
    (void)self;
    void* ptr = malloc(size);
    if (ptr != NULL) memset(ptr, 0, size);
    return ptr;
}

void* alloc_realloc(allocator_t* self, void* ptr, usize old_size, usize new_size) {
    (void)self, (void)old_size;
    return realloc(ptr, new_size);
}

void alloc_free(allocator_t* self, void* ptr, usize size) {
    (void)self;
    memset(ptr, 0xDE, size);
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

// FIXME: return alloc error
arena_t arena_init_alloc(allocator_t* alloc, usize size) {
    assert(alloc != NULL);
    u8* ptr = alloc_alloc(alloc, size * sizeof(u8));
    assert(ptr != NULL);

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
        memset(self->ptr_, 0xDE, self->capacity);
        break;
    case ARENA_BACKING_ALLOC:
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
    if (self->index + size > self->capacity) return NULL;

    // Assumes that the original buffer and it's internal tail index are always aligned to
    // `max_align_t`
    void* ptr = self->ptr_ + self->index;
    self->index = (u32)align_forward(self->index + size, alignof(max_align_t));
    memset(ptr, 0, size);
    return ptr;
}

void* arena_realloc(arena_t* self, void* ptr, usize old_size, usize new_size) {
    if (arena_try_resize(self, ptr, old_size, new_size)) return ptr;

    void* new_ptr = arena_alloc(self, new_size);
    if (new_ptr != NULL) {
        memcpy(new_ptr, ptr, old_size);
        memset(ptr, 0xDE, old_size);
    }
    return new_ptr;
}

bool arena_try_resize(arena_t* self, void* ptr, usize old_size, usize new_size) {
    u8* start = ptr;
    assert(self != NULL);
    assert(self->ptr_ <= start);
    assert(start + old_size <= self->ptr_ + self->index);

    if (start + old_size < self->ptr_ + self->index) return false;
    if (start + new_size > self->ptr_ + self->capacity) return false;

    if (new_size > old_size) {
        self->index += new_size - old_size;
    } else {
        self->index -= old_size - new_size;
    }
    return true;
}
