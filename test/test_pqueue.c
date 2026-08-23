#include "common.h"
#include "pqueue.h"
#include "testing.h"

#define MAX_CAPACITY 256
#define ITER_COUNT   100

static void test_pqueue_build_and_drain(rng_t* rng) {
    BEGIN_TEST("test_pqueue_build_and_drain");

    for (usize i = 0; i < ITER_COUNT; ++i) {
        u32 items[MAX_CAPACITY] = { 0 };
        for (usize j = 0; j < array_len(items); ++j) {
            items[j] = rng_u32(rng);
        }
        u32 curr, prev;
        bool has_prev = false;

        pqueue_u32_t pqueue = pqueue_u32_from_fixed(items, array_len(items));
        assert_false(pqueue_u32_push(&pqueue, 69));

        for (usize j = 0; j < array_len(items); ++j) {
            assert_true(pqueue_u32_pop(&pqueue, &curr));
            if (has_prev) {
                assert_u32(prev, <=, curr);
            }
            prev = curr;
            has_prev = true;
        }
        assert_false(pqueue_u32_peek(&pqueue, NULL));
        assert_false(pqueue_u32_pop(&pqueue, NULL));

        pqueue_u32_release(&pqueue);
    }
    END_TEST();
}
