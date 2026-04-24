#include "../common.h"
#include "../darray.h"
#include "../pqueue.h"
#include "../testing.h"

#define MAX_CAPACITY 1024
#define ITER_COUNT   100

void test_pqueue_build_and_drain(rng_t* rng) {
    BEGIN_TEST("test_pqueue_build_and_drain");

    for (usize i = 0; i < ITER_COUNT; ++i) {
        u32 items[MAX_CAPACITY] = { 0 };
        for (usize i = 0; i < MAX_CAPACITY; ++i) {
            items[i] = rng_u32(rng);
        }
        pqueue_u32_t pqueue = pqueue_u32_from_fixed(items, MAX_CAPACITY);
        u32 curr, prev;
        bool has_prev = false;

        while (pqueue_u32_pop(&pqueue, &curr)) {
            if (has_prev) {
                assert_u32(prev, <=, curr);
            }
            prev = curr;
            has_prev = true;
        }
        pqueue_u32_release(&pqueue);
    }
    END_TEST();
}

typedef enum {
    ACTION_PUSH,
    ACTION_PEEK,
    ACTION_POP,
    ACTION_COUNT,
} action_kind_t;

static action_kind_t fuzz_read_weighted_action(fuzz_reader_t* input) {
    u8 value = fuzz_read_u8(input) % 5;
    if (value < 3) return ACTION_PUSH; // NOTE: Biased so it tends to grow over time
    if (value < 4) return ACTION_PEEK;
    if (value < 5) return ACTION_POP;
    unreachable;
}

static usize find_min(darray_u32_t oracle) {
    usize min_idx = 0;
    for (usize idx = 0; idx < oracle.len; idx++) {
        if (array_at(oracle, idx) < array_at(oracle, min_idx)) min_idx = idx;
    }
    return min_idx;
}

void fuzz_pqueue(test_grow_policy_t grow, fuzz_reader_t input) {
    BEGIN_TEST("fuzz_pqueue");
    SETUP_TEST(pqueue, pqueue_u32, u32, grow);

    u32 oracle_buf[MAX_CAPACITY] = { 0 };
    darray_u32_t oracle = darray_u32_init_fixed(oracle_buf, MAX_CAPACITY);
    u32 item, actual, expected;
    bool has_act, has_exp;

    usize it = 0;
    while (!fuzz_reader_is_empty(input)) {
        if ((it++ & 31) == 0) {
            has_act = pqueue_u32_peek(&pqueue, &actual);
            has_exp = oracle.len > 0;
            assert_u8(has_act, ==, has_exp);
            if (has_act) assert_u32(actual, ==, array_at(oracle, find_min(oracle)));
        }

        switch (fuzz_read_weighted_action(&input)) {
        case ACTION_PUSH:
            item = fuzz_read_u32(&input);
            if (darray_u32_push(&oracle, item)) {
                assert(pqueue_u32_push(&pqueue, item));
            }
            break;
        case ACTION_PEEK:
            has_act = pqueue_u32_peek(&pqueue, &actual);
            has_exp = oracle.len > 0;
            assert_u8(has_act, ==, has_exp);
            if (has_act) assert_u32(actual, ==, array_at(oracle, find_min(oracle)));
            break;
        case ACTION_POP: {
            has_act = pqueue_u32_pop(&pqueue, &actual);
            has_exp = oracle.len > 0;
            assert_u8(has_act, ==, has_exp);
            if (has_act) assert_u32(actual, ==, darray_u32_swap_remove(&oracle, find_min(oracle)));
            break;
        }
        case ACTION_COUNT:
            unreachable;
        }
    }

    while (true) {
        has_act = pqueue_u32_pop(&pqueue, &actual);
        has_exp = oracle.len > 0;
        assert_u8(has_act, ==, has_exp);
        if (has_act) assert_u32(actual, ==, darray_u32_swap_remove(&oracle, find_min(oracle)));
        else break;
    }
    pqueue_u32_release(&pqueue);
    END_TEST();
}

int LLVMFuzzerTestOneInput(const u8* data, usize size) {
    fuzz_reader_t input = { .ptr = data, .len = size };

    u32 fixed_buf[MAX_CAPACITY] = { 0 };
    u32 arena_buf[MAX_CAPACITY] = { 0 };
    arena_t arena = arena_init_fixed(arena_buf, sizeof(arena_buf));
    allocator_t alloc;

    test_grow_policy_t grow_fixed = {
        .buffer = fixed_buf,
        .size = sizeof(fixed_buf),
        .kind = GROW_POLICY_FIXED,
    };
    test_grow_policy_t grow_arena = {
        .arena = &arena,
        .kind = GROW_POLICY_ARENA,
    };
    test_grow_policy_t grow_alloc = {
        .alloc = &alloc,
        .kind = GROW_POLICY_ALLOC,
    };
    fuzz_pqueue(grow_fixed, input);
    fuzz_pqueue(grow_arena, input);
    fuzz_pqueue(grow_alloc, input);
    return EXIT_SUCCESS;
}
