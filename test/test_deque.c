#define GENERICS_IMPLEMENTATION
#include <core/deque.h>
#include <core/testing.h>

DEQUE_DECL(i32)
DEQUE_IMPL(i32)

static void test_deque_basic(testing_context test) {
    deque_u32 deque = deque_u32_init_alloc(test.allocator);

    assert_false(deque_u32_pop_front(&deque, NULL));
    assert_false(deque_u32_pop_back (&deque, NULL));

    assert_true(deque_u32_push_back(&deque, 1));
    assert_true(deque_u32_push_back(&deque, 2));
    assert_true(deque_u32_push_back(&deque, 3));
    assert_true(deque_u32_push_front(&deque, 0));

    u32 actual;
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 0);
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 1);
    assert_true(deque_u32_pop_back (&deque, &actual)); assert_u32(actual, ==, 3);
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 2);

    assert_false(deque_u32_pop_front(&deque, &actual));
    assert_false(deque_u32_pop_back (&deque, &actual));
    deque_u32_destroy(&deque);
}

static void test_deque_fixed(testing_context test) {
    u32* buffer = arena_alloc(&test.arena, u32, 4);
    deque_u32 deque = deque_u32_init_fixed(buffer, 4);

    assert_false(deque_u32_pop_front(&deque, NULL));
    assert_false(deque_u32_pop_back (&deque, NULL));

    assert_true(deque_u32_push_back (&deque, 1));
    assert_true(deque_u32_push_back (&deque, 2));
    assert_true(deque_u32_push_back (&deque, 3));
    assert_true(deque_u32_push_front(&deque, 0));
    assert_false(deque_u32_push_back(&deque, 69));

    u32 actual;
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 0);
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 1);
    assert_true(deque_u32_pop_back (&deque, &actual)); assert_u32(actual, ==, 3);
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 2);

    assert_false(deque_u32_pop_front(&deque, &actual));
    assert_false(deque_u32_pop_back (&deque, &actual));
}

static void test_deque_slow_growth(testing_context test) {
    deque_i32 deque = deque_i32_init_alloc(test.allocator);

    assert_true(deque_i32_reserve_exact(&deque, 1)); assert_u32(deque.capacity, >=, 1);
    assert_true(deque_i32_push_back (&deque, 1));
    assert_true(deque_i32_reserve_exact(&deque, 2)); assert_u32(deque.capacity, >=, 2);
    assert_true(deque_i32_push_front(&deque, 0));
    assert_true(deque_i32_reserve_exact(&deque, 3)); assert_u32(deque.capacity, >=, 3);
    assert_true(deque_i32_push_back (&deque, 2));
    assert_true(deque_i32_reserve_exact(&deque, 5)); assert_u32(deque.capacity, >=, 5);
    assert_true(deque_i32_push_back (&deque, 3));
    assert_true(deque_i32_push_front(&deque, -1));
    assert_true(deque_i32_reserve_exact(&deque, 6)); assert_u32(deque.capacity, >=, 6);
    assert_true(deque_i32_push_front(&deque, -2));

    i32 actual;
    assert_true(deque_i32_pop_front(&deque, &actual)); assert_i32(actual, ==, -2);
    assert_true(deque_i32_pop_front(&deque, &actual)); assert_i32(actual, ==, -1);
    assert_true(deque_i32_pop_back (&deque, &actual)); assert_i32(actual, ==, 3);
    assert_true(deque_i32_pop_front(&deque, &actual)); assert_i32(actual, ==, 0);
    assert_true(deque_i32_pop_back (&deque, &actual)); assert_i32(actual, ==, 2);
    assert_true(deque_i32_pop_back (&deque, &actual)); assert_i32(actual, ==, 1);

    assert_false(deque_i32_pop_front(&deque, &actual));
    assert_false(deque_i32_pop_back(&deque, &actual));
    deque_i32_destroy(&deque);
}

static void test_deque_push_many(testing_context test) {
    deque_u32 deque = deque_u32_init_alloc(test.allocator);

    assert_true(deque_u32_push_back_many (&deque, (u32[]) { 3, 4, 5 }, 3));
    assert_true(deque_u32_push_back_many (&deque, (u32[]) { 6, 7    }, 2));
    assert_true(deque_u32_push_front_many(&deque, (u32[]) { 2       }, 1));
    assert_true(deque_u32_push_back_many (&deque, NULL, 0));
    assert_true(deque_u32_push_front_many(&deque, (u32[]) { 0, 1    }, 2));
    assert_true(deque_u32_push_front_many(&deque, NULL, 0));

    u32 actual;
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 0);
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 1);
    assert_true(deque_u32_pop_back (&deque, &actual)); assert_u32(actual, ==, 7);
    assert_true(deque_u32_pop_back (&deque, &actual)); assert_u32(actual, ==, 6);

    assert_true(deque_u32_push_front_many(&deque, (u32[]) { 0, 1 }, 2));
    assert_true(deque_u32_push_back_many (&deque, (u32[]) { 6, 7 }, 2));

    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 0);
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 1);
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 2);
    assert_true(deque_u32_pop_back (&deque, &actual)); assert_u32(actual, ==, 7);
    assert_true(deque_u32_pop_back (&deque, &actual)); assert_u32(actual, ==, 6);
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 3);
    assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, 4);
    assert_true(deque_u32_pop_back (&deque, &actual)); assert_u32(actual, ==, 5);

    assert_false(deque_u32_pop_front(&deque, &actual));
    assert_false(deque_u32_pop_back(&deque, &actual));
    deque_u32_destroy(&deque);
}

static void test_deque_fifo(testing_context test) {
    const u32 items[] = { 0, 1, 2, 3, 4, 5 };
    deque_u32 deque = deque_u32_init_alloc(test.allocator);
    u32 actual;

    assert_true(deque_u32_push_front_many(&deque, items, array_len(items)));
    for (usize i = 0; i < array_len(items); ++i) {
        assert_true(deque_u32_back    (&deque, &actual)); assert_u32(actual, ==, items[i]);
        assert_true(deque_u32_pop_back(&deque, &actual)); assert_u32(actual, ==, items[i]);
    }
    assert_false(deque_u32_back    (&deque, NULL));
    assert_false(deque_u32_pop_back(&deque, NULL));

    assert_true(deque_u32_push_back_many(&deque, items, array_len(items)));
    for (usize i = array_len(items); i-- > 0;) {
        assert_true(deque_u32_front    (&deque, &actual)); assert_u32(actual, ==, items[i]);
        assert_true(deque_u32_pop_front(&deque, &actual)); assert_u32(actual, ==, items[i]);
    }
    assert_false(deque_u32_front    (&deque, NULL));
    assert_false(deque_u32_pop_front(&deque, NULL));
    deque_u32_destroy(&deque);
}

const testing_case test_suite_deque[] = {
    { "test_deque_basic",       &test_deque_basic       },
    { "test_deque_fixed",       &test_deque_fixed       },
    { "test_deque_slow_growth", &test_deque_slow_growth },
    { "test_deque_push_many",   &test_deque_push_many   },
    { "test_deque_fifo",        &test_deque_fifo        },
};
