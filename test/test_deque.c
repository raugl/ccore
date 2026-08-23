#include "common.h"
#include "deque.h"
#include "testing.h"

#define MAX_CAPACITY 256

static void test_deque_basic(test_grow_policy_t grow) {
    BEGIN_TEST("test_deque_basic");
    SETUP_TEST(deque, deque_u32, u32, grow);

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

    // TODO: Create a failing test_grow_policy_t
    if (grow.kind == GROW_POLICY_FIXED) {
        u32 item = 0;
        assert_false(deque_u32_push_back_many(&deque, &item, grow.size / sizeof(u32)));
        assert_false(deque_u32_push_front_many(&deque, &item, grow.size / sizeof(u32)));
    }
    deque_u32_release(&deque);
    END_TEST();
}

static void test_deque_push_many(test_grow_policy_t grow) {
    BEGIN_TEST("test_deque_push_many");
    SETUP_TEST(deque, deque_u32, u32, grow);

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
    END_TEST();
}

static void test_deque_fifo(test_grow_policy_t grow) {
    BEGIN_TEST("test_deque_iterate");
    SETUP_TEST(deque, deque_u32, u32, grow);

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
    END_TEST();
}

void test_deque(void);

// FIXME: The arena is not getting reset between tests
void test_deque(void) {
    u32 fixed_buf[MAX_CAPACITY] = { 0 };
    test_grow_policy_t grow_fixed = {
        .buffer = fixed_buf,
        .size = sizeof(fixed_buf),
        .kind = GROW_POLICY_FIXED,
    };
    test_deque_basic(grow_fixed);
    // test_deque_growing(grow_fixed);
    test_deque_push_many(grow_fixed);
    test_deque_fifo(grow_fixed);

    u32 arena_buf[MAX_CAPACITY] = { 0 };
    arena_t arena = arena_init_fixed(arena_buf, sizeof(arena_buf));
    test_grow_policy_t grow_arena = {
        .arena = &arena,
        .kind = GROW_POLICY_ARENA,
    };
    test_deque_basic(grow_arena);
    // test_deque_growing(grow_arena);
    test_deque_push_many(grow_arena);
    test_deque_fifo(grow_arena);

    allocator_t alloc;
    test_grow_policy_t grow_alloc = {
        .alloc = &alloc,
        .kind = GROW_POLICY_ALLOC,
    };
    test_deque_basic(grow_alloc);
    // test_deque_growing(grow_alloc);
    test_deque_push_many(grow_alloc);
    test_deque_fifo(grow_alloc);
}
