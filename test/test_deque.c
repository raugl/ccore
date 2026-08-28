#include <core/common.h>
#include <core/deque.h>
#include <core/testing.h>

// TODO: Add a test for init fixed

static void test_deque_basic(testing_context test) {
    deque_u32 deque = deque_u32_init_alloc(test.allocator);

    assert_false(deque_u32_pop_front(&deque, NULL));
    assert_false(deque_u32_pop_back(&deque, NULL));

    assert_true(deque_u32_push_back(&deque, 1));
    assert_true(deque_u32_push_back(&deque, 2));
    assert_true(deque_u32_push_back(&deque, 3));
    assert_true(deque_u32_push_front(&deque, 0));

    u32 actual;
    assert_true(deque_u32_pop_front(&deque, &actual));
    assert_u32(actual, ==, 0);
    assert_true(deque_u32_pop_front(&deque, &actual));
    assert_u32(actual, ==, 1);
    assert_true(deque_u32_pop_back(&deque, &actual));
    assert_u32(actual, ==, 3);
    assert_true(deque_u32_pop_front(&deque, &actual));
    assert_u32(actual, ==, 2);

    assert_false(deque_u32_pop_front(&deque, &actual));
    assert_false(deque_u32_pop_back(&deque, &actual));

    test.debug_alloc.fail_after_n = 0;
    assert_false(deque_u32_push_back_many(&deque, deque._ptr, deque.capacity + 1));

    test.debug_alloc.fail_after_n = 0;
    assert_false(deque_u32_push_front_many(&deque, deque._ptr, deque.capacity + 1));
    deque_u32_release(&deque);
}

static void test_deque_push_many(testing_context test) {
    deque_u32 deque = deque_u32_init_alloc(test.allocator);

    assert_true(deque_u32_push_back_many(&deque, (u32[]) { 3, 4, 5 }, 3));
    assert_true(deque_u32_push_back_many(&deque, (u32[]) { 6, 7 }, 2));
    assert_true(deque_u32_push_front_many(&deque, (u32[]) { 2 }, 1));
    assert_true(deque_u32_push_back_many(&deque, NULL, 0));
    assert_true(deque_u32_push_front_many(&deque, (u32[]) { 0, 1 }, 2));
    assert_true(deque_u32_push_front_many(&deque, NULL, 0));

    u32 actual;
    assert_true(deque_u32_pop_front(&deque, &actual));
    assert_u32(actual, ==, 0);
    assert_true(deque_u32_pop_front(&deque, &actual));
    assert_u32(actual, ==, 1);
    assert_true(deque_u32_pop_back(&deque, &actual));
    assert_u32(actual, ==, 7);
    assert_true(deque_u32_pop_back(&deque, &actual));
    assert_u32(actual, ==, 6);

    assert_true(deque_u32_push_front_many(&deque, (u32[]) { 0, 1 }, 2));
    assert_true(deque_u32_push_back_many(&deque, (u32[]) { 6, 7 }, 2));

    assert_true(deque_u32_pop_front(&deque, &actual));
    assert_u32(actual, ==, 0);
    assert_true(deque_u32_pop_front(&deque, &actual));
    assert_u32(actual, ==, 1);
    assert_true(deque_u32_pop_front(&deque, &actual));
    assert_u32(actual, ==, 2);
    assert_true(deque_u32_pop_back(&deque, &actual));
    assert_u32(actual, ==, 7);
    assert_true(deque_u32_pop_back(&deque, &actual));
    assert_u32(actual, ==, 6);
    assert_true(deque_u32_pop_front(&deque, &actual));
    assert_u32(actual, ==, 3);
    assert_true(deque_u32_pop_front(&deque, &actual));
    assert_u32(actual, ==, 4);
    assert_true(deque_u32_pop_back(&deque, &actual));
    assert_u32(actual, ==, 5);

    assert_false(deque_u32_pop_front(&deque, &actual));
    assert_false(deque_u32_pop_back(&deque, &actual));
    deque_u32_release(&deque);
}

static void test_deque_fifo(testing_context test) {
    deque_u32 deque = deque_u32_init_alloc(test.allocator);

    const u32 items[] = { 0, 1, 2, 3, 4, 5 };
    assert_true(deque_u32_push_front_many(&deque, items, array_len(items)));

    u32 actual;
    for (usize i = 0; i < array_len(items); ++i) {
        assert_true(deque_u32_back(&deque, &actual));
        assert_u32(actual, ==, items[i]);

        assert_true(deque_u32_pop_back(&deque, &actual));
        assert_u32(actual, ==, items[i]);
    }
    assert_false(deque_u32_back(&deque, NULL));
    assert_false(deque_u32_pop_back(&deque, NULL));

    assert_true(deque_u32_push_back_many(&deque, items, array_len(items)));
    for (usize i = array_len(items); i-- > 0;) {
        assert_true(deque_u32_front(&deque, &actual));
        assert_u32(actual, ==, items[i]);

        assert_true(deque_u32_pop_front(&deque, &actual));
        assert_u32(actual, ==, items[i]);
    }
    assert_false(deque_u32_front(&deque, NULL));
    assert_false(deque_u32_pop_front(&deque, NULL));
    deque_u32_release(&deque);
}

const testing_case test_suite_deque[] = {
    { "test_deque_basic",     &test_deque_basic     },
    { "test_deque_push_many", &test_deque_push_many },
    { "test_deque_fifo",      &test_deque_fifo      },
};
