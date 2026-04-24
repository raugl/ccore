#define GENERICS_IMPLEMENTATION
#include "../common.h"
#include "../darray.h"
#include "../pqueue.h"
#include "../testing.h"

#define MAX_CAPACITY 1024

typedef enum {
    ACTION_PUSH,
    ACTION_PEEK,
    ACTION_POP,
    ACTION_COUNT,
} action_kind_t;

static usize find_min(darray_u32_t oracle) {
    usize min_idx = 0;
    for (usize idx = 0; idx < oracle.len; idx++) {
        if (array_at(oracle, idx) < array_at(oracle, min_idx)) min_idx = idx;
    }
    return min_idx;
}

void fuzz_pqueue(pqueue_u32_t* pqueue, rng_t* rng, fuzz_reader_t input) {
    u32 buffer[MAX_CAPACITY] = { 0 };
    darray_u32_t oracle = darray_u32_init_fixed(buffer, MAX_CAPACITY);

    while (!fuzz_reader_is_empty(input)) {
        u32 item, actual, expected;

        switch (fuzz_read_u8(&input) % ACTION_COUNT) {
        case ACTION_PUSH:
            // NOTE: consume even if would skip, to preserve a consistent message structure
            item = fuzz_read_u32(&input);
            if (pqueue->len == MAX_CAPACITY) {
                continue;
            }
            assert(darray_u32_push(&oracle, item));
            assert(pqueue_u32_push(pqueue, item));
            break;
        case ACTION_PEEK:
            assert_u32(pqueue->len, ==, oracle.len);
            if (pqueue->len > 0) {
                assert_u32(pqueue_peek(*pqueue), ==, array_at(oracle, find_min(oracle)));
            }
            break;
        case ACTION_POP: {
            // TODO: swap remove
            bool popped = pqueue_u32_pop(pqueue, &actual);
            bool removed = darray_u32_remove(&oracle, find_min(oracle), &expected);
            assert_u8(popped, ==, removed);

            if (popped) assert_u32(actual, ==, expected);
            break;
        }
        case ACTION_COUNT:
            unreachable;
        }
        if (rng_range_int(rng, 0, 32)) {
            for (usize idx = 1; idx < pqueue->len; ++idx) {
                u32 node = array_at(*pqueue, idx);
                u32 parent = array_at(*pqueue, bt_parent(idx));
                assert_u32(node, >=, parent);
            }
        }
    }
    for (usize idx = 1; idx < pqueue->len; ++idx) {
        u32 node = array_at(*pqueue, idx);
        u32 parent = array_at(*pqueue, bt_parent(idx));
        assert_u32(node, >=, parent);
    }
}

int LLVMFuzzerTestOneInput(const u8* data, usize size) {
    fuzz_reader_t input = { .ptr = data, .len = size };
    rng_t rng = rng_seed(fuzz_read_u64(&input));
    {
        u32 buffer[MAX_CAPACITY] = { 0 };
        pqueue_u32_t pqueue = pqueue_u32_init_fixed(buffer, MAX_CAPACITY);
        fuzz_pqueue(&pqueue, &rng, input);
    }
    {
        u32 buffer[MAX_CAPACITY * 4] = { 0 };
        arena_t arena = arena_init_fixed(buffer, sizeof(buffer));
        pqueue_u32_t pqueue = pqueue_u32_init_arena(&arena, 0);
        fuzz_pqueue(&pqueue, &rng, input);
    }
    {
        allocator_t alloc = {};
        pqueue_u32_t pqueue = pqueue_u32_init_alloc(&alloc, 0);
        fuzz_pqueue(&pqueue, &rng, input);
    }
    return EXIT_SUCCESS;
}
