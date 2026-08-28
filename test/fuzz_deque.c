#include <core/slice.h>
#include <core/deque.h>
#include <core/darray.h>
#include <core/testing.h>

#define MAX_CAPACITY 256

typedef struct fuzz_allocator {
    fuzz_reader* input;
    allocator_t  backing;
    bool         used[2];
    usize        sizes[2];
    slice_u8     buffers[2];
} fuzz_allocator;

static bool fuzz_allocator_init(fuzz_allocator* self, fuzz_reader* input, allocator_t backing) {
    assert(self != NULL);

    fuzz_allocator fuzz = {
        .input          = input,
        .backing        = backing,
        .buffers[0].len = MAX_CAPACITY,
        .buffers[1].len = MAX_CAPACITY,
    };
    if (!(fuzz.buffers[0].ptr = mem_alloc(backing, u32, fuzz.buffers[0].len))) goto cleanup;
    if (!(fuzz.buffers[1].ptr = mem_alloc(backing, u32, fuzz.buffers[1].len))) goto cleanup;

    *self = fuzz;
    return true;

cleanup:
    mem_free(backing, fuzz.buffers[0].ptr, fuzz.buffers[0].len);
    mem_free(backing, fuzz.buffers[1].ptr, fuzz.buffers[1].len);
    return false;
}

static u8 fuzz_allocator_alloc_count(fuzz_allocator self) {
    return self.used[0] + self.used[1];
}

static bool fuzz_allocator_release(fuzz_allocator* self, bool log_leaks) {
    assert(self != NULL);
    u8 leaked_count = fuzz_allocator_alloc_count(*self);

    if (log_leaks && leaked_count > 0) {
        log_error("Leaked %d fuzz allocator allocations!", leaked_count);
    }
    mem_free(self->backing, self->buffers[0].ptr, self->buffers[0].len);
    mem_free(self->backing, self->buffers[1].ptr, self->buffers[1].len);

    memset_destroyed(self, sizeof(fuzz_allocator));
    return (leaked_count > 0);
}

static u8 fuzz_allocator_mem_slot(fuzz_allocator self, void* mem, usize size) {
    u8 slot = (mem == self.buffers[1].ptr);
    assert(mem == self.buffers[slot].ptr);
    assert(self.used[slot]);
    assert(self.sizes[slot] == size);
    return slot;
}

static void* fuzz_allocator_proc(allocator_operation op, void* self_, void* old_ptr, usize old_size, usize new_size, usize align) {
    fuzz_allocator* self = self_;
    assert(align == 4);

    switch (op) {
    case ALLOCATOR_OPERATION_ALLOC: {
        u8 slot = (self->used[0]);
        assert(!self->used[slot]);
        assert(new_size % 4 == 0);

        if (new_size > self->buffers[slot].len) return NULL;

        self->used[slot] = true;
        self->sizes[slot] = new_size;
        return self->buffers[slot].ptr;
    }
    case ALLOCATOR_OPERATION_REALLOC: {
        assert(old_size <= self->buffers[0].len);
        assert(fuzz_allocator_alloc_count(*self) == 1);

        u8 slot = fuzz_allocator_mem_slot(*self, old_ptr, old_size);
        if (new_size > self->buffers[slot].len || fuzz_read_bool(self->input)) return NULL;

        bool grow_in_place = fuzz_read_bool(self->input);
        if (grow_in_place) {
            self->sizes[slot] = new_size;
            return old_ptr;
        } else {
            u8 new_slot = ~slot & 0x3;
            self->used[slot] = false;
            self->used[new_slot] = true;
            self->sizes[new_slot] = new_size;

            void* new_ptr = self->buffers[new_slot].ptr;
            memcpy(new_ptr, old_ptr, min_usize(old_size, new_size));
            return new_ptr;
        }
    }
    case ALLOCATOR_OPERATION_RESIZE: {
        assert(fuzz_allocator_alloc_count(*self) == 1);

        u8 slot = fuzz_allocator_mem_slot(*self, old_ptr, old_size);
        if (new_size > self->buffers[slot].len || fuzz_read_bool(self->input)) return NULL;

        self->sizes[slot] = new_size;
        return (void*)alignof(max_align_t);
    }
    case ALLOCATOR_OPERATION_FREE: {
        u8 slot = fuzz_allocator_mem_slot(*self, old_ptr, old_size);
        self->used[slot] = false;
        self->sizes[slot] = 0;
        return NULL;
    }
    }
}

static allocator_t allocator_init_fuzz(fuzz_allocator* fuzz) {
    return (allocator_t) { .self = fuzz, .proc = &fuzz_allocator_proc };
}

typedef enum action_kind {
    ACTION_GROW,
    ACTION_PUSH_BACK,
    ACTION_PUSH_FRONT,
    ACTION_PUSH_BACK_MANY,
    ACTION_PUSH_FRONT_MANY,
    ACTION_POP_BACK,
    ACTION_POP_FRONT,
} action_kind;

// NOTE: The modulo introduces a tiny bias. Values in the range [0, 5] are sampled 4% more
// often than values in the range [6, 9], compared to what the weights table might suggest.
static action_kind fuzz_read_weighted_action(fuzz_reader* input) {
    static const action_kind weights[10] = {
        ACTION_GROW, ACTION_GROW, ACTION_GROW, ACTION_GROW,
        ACTION_PUSH_BACK,
        ACTION_PUSH_FRONT,
        ACTION_PUSH_BACK_MANY,
        ACTION_PUSH_FRONT_MANY,
        ACTION_POP_BACK,
        ACTION_POP_FRONT,
    };
    return weights[fuzz_read_u8(input) % array_len(weights)];
}

static void fuzz_deque(testing_context test, fuzz_reader input) {
    fuzz_allocator fuzz_alloc;
    assert_true(fuzz_allocator_init(&fuzz_alloc, &input, test.allocator));
    deque_u32 deque = deque_u32_init_alloc(allocator_init_fuzz(&fuzz_alloc));

    u32* buffer = mem_alloc(test.allocator, u32, MAX_CAPACITY);
    darray_u32 oracle = darray_u32_init_fixed(buffer, MAX_CAPACITY);

    u32 item, items[8], actual, expected;
    while (!fuzz_reader_is_empty(input)) {
        switch (fuzz_read_weighted_action(&input)) {
        case ACTION_PUSH_BACK:
            item = fuzz_read_u32(&input);
            assert_u8(deque_u32_push_back(&deque, item), ==, darray_u32_push(&oracle, item));
            break;
        case ACTION_PUSH_FRONT:
            item = fuzz_read_u32(&input);
            assert_u8(deque_u32_push_front(&deque, item), ==, darray_u32_insert(&oracle, 0, item));
            break;
        case ACTION_PUSH_BACK_MANY: {
            u8 items_count = fuzz_read_u8(&input) & 7;
            for (u8 i = 0; i < items_count; ++i) {
                items[i] = fuzz_read_u32(&input);
            }
            assert_u8(
                deque_u32_push_back_many(&deque, items, items_count), ==,
                darray_u32_push_many(&oracle, items, items_count)
            );
            break;
        }
        case ACTION_PUSH_FRONT_MANY: {
            u8 items_count = fuzz_read_u8(&input) & 7;
            for (u8 i = 0; i < items_count; ++i) {
                items[i] = fuzz_read_u32(&input);
            }
            assert_u8(
                deque_u32_push_front_many(&deque, items, items_count), ==,
                darray_u32_insert_many(&oracle, 0, items, items_count)
            );
            break;
        }
        case ACTION_POP_BACK:
            actual = expected = 0;
            assert_u8(deque_u32_pop_back(&deque, &actual), ==, darray_u32_pop(&oracle, &expected));
            assert_u32(actual, ==, expected);
            break;
        case ACTION_POP_FRONT:
            assert_u8(deque_u32_pop_front(&deque, &actual), ==, oracle.len > 0);
            if (oracle.len > 0) {
                expected = darray_u32_remove(&oracle, 0);
                assert_u32(actual, ==, expected);
            }
            break;
        case ACTION_GROW: {
            const u8 growth = fuzz_read_u8(&input) & 7;
            assert_u8(
                deque_u32_reserve_exact(&deque, deque.count + growth), ==,
                darray_u32_ensure_capacity(&oracle, oracle.len + growth)
            );
            break;
        }
        }
    }
    actual = expected = 0;
    assert_u8(deque_u32_back(&deque, &actual), ==, darray_u32_back(&oracle, &expected));
    assert_u32(actual, ==, expected);

    assert_u8(deque_u32_front(&deque, &actual), ==, oracle.len > 0);
    if (oracle.len > 0) assert_u32(actual, ==, oracle.ptr[0]);

    assert_u32(deque.count, ==, oracle.len);
    assert_u32(deque.capacity, ==, oracle.capacity);

    for (usize i = 0; i < oracle.len; ++i) {
        assert_true(deque_u32_pop_front(&deque, &actual));
        assert_u32(actual, ==, oracle.ptr[i]);
    }
    assert_u32(deque.count, ==, oracle.len);

    deque_u32_destroy(&deque);
    mem_free(test.allocator, buffer, MAX_CAPACITY);
    assert_false(fuzz_allocator_release(&fuzz_alloc, true));
}

int LLVMFuzzerTestOneInput(const u8* data, usize size) {
    fuzz_reader input = {
        .ptr = data,
        .len = size,
    };
    rng_t rng = rng_seed(fuzz_read_u64(&input));
    arena_t arena = arena_init_alloc(allocator_init_malloc(), 16 * 1024);
    allocator_t alloc = allocator_init_arena(&arena);

    testing_context ctx = {
        .rng          = &rng, // NOTE: I don't use `test.arena`, so I didn't bother handing it memory
        .allocator    = allocator_init_debug(&ctx._debug_alloc),
        ._debug_alloc = debug_allocator_init(alloc, alloc),
    };
    fuzz_deque(ctx, input);

    assert_false(debug_allocator_release(&ctx._debug_alloc, true));
    arena_release(&arena);
    return EXIT_SUCCESS;
}
