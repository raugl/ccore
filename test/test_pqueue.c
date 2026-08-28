#include <core/common.h>
#include <core/random.h>
#include <core/pqueue.h>
#include <core/testing.h>

#define MAX_CAPACITY 256
#define ITER_COUNT   100

static void test_pqueue_build_and_drain(testing_context test) {
    for (usize i = 0; i < ITER_COUNT; ++i) {

        u32 items[MAX_CAPACITY] = { 0 };
        for (usize j = 0; j < array_len(items); ++j) {
            items[j] = rng_u32(test.rng);
        }
        pqueue_u32 pqueue = pqueue_u32_init_fixed(items, array_len(items));
        assert_false(pqueue_u32_push(&pqueue, 69));

        u32 curr, prev;
        bool has_prev = false;

        for (usize j = 0; j < array_len(items); ++j) {
            assert_true(pqueue_u32_pop(&pqueue, &curr));
            if (has_prev) assert_u32(prev, <=, curr);

            prev = curr;
            has_prev = true;
        }

        assert_false(pqueue_u32_peek(&pqueue, NULL));
        assert_false(pqueue_u32_pop(&pqueue, NULL));
        pqueue_u32_release(&pqueue);
    }
}

const testing_case test_suite_pqueue[] = {
    { "test_pqueue_build_and_drain", &test_pqueue_build_and_drain },
};
