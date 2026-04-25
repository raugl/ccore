#include "common.h"
#include "darray.h"
#include "slice.h"
#include "testing.h"

#define MAX_CAPACITY 256

static void test_darray_basic(test_grow_policy_t grow) {
    BEGIN_TEST("test_darray_basic");
    SETUP_TEST(arr, darray_u32, u32, grow);

    for (u32 i = 0; i < 10; ++i) {
        assert_true(darray_u32_push(&arr, i + 1));
    }
    for (u32 i = 0; i < 10; ++i) {
        assert_u32(array_at(arr, i), ==, i + 1);
    }

    u32 actual;
    assert_true(darray_u32_pop(&arr, &actual));
    assert_u32(actual, ==, 10);
    assert_size(arr.len, ==, 9);

    u32 many[] = { 1, 2, 3 };
    assert_true(darray_u32_push_many(&arr, many, 3));
    assert_size(arr.len, ==, 12);
    assert_true(darray_u32_pop(&arr, &actual));
    assert_u32(actual, ==, 3);
    assert_true(darray_u32_pop(&arr, &actual));
    assert_u32(actual, ==, 2);
    assert_true(darray_u32_pop(&arr, &actual));
    assert_u32(actual, ==, 1);
    assert_size(arr.len, ==, 9);

    array_at(arr, 7) = 69;
    array_at(arr, 8) = 42;

    assert_true(darray_u32_pop(&arr, &actual));
    assert_u32(actual, ==, 42);

    assert_true(darray_u32_pop(&arr, &actual));
    assert_u32(actual, ==, 69);

    darray_u32_release(&arr);
    END_TEST();
}

static void test_darray_ordered_remove(test_grow_policy_t grow) {
    BEGIN_TEST("test_darray_ordered_remove");
    SETUP_TEST(arr, darray_u32, u32, grow);

    for (u32 i = 0; i < 10; ++i) {
        assert_true(darray_u32_push(&arr, i));
    }

    // Remove from the middle
    assert_u32(darray_u32_remove(&arr, 3), ==, 3);
    assert_u32(array_at(arr, 3), ==, 4);
    assert_u32(arr.len, ==, 9);

    // Remove from the end
    assert_u32(darray_u32_remove(&arr, 8), ==, 9);
    assert_u32(arr.len, ==, 8);

    // Remove from the front
    assert_u32(darray_u32_remove(&arr, 0), ==, 0);
    assert_u32(array_at(arr, 0), ==, 1);
    assert_u32(arr.len, ==, 7);

    darray_u32_release(&arr);
    END_TEST();
}

static void test_darray_swap_remove(test_grow_policy_t grow) {
    BEGIN_TEST("test_darray_swap_remove");
    SETUP_TEST(arr, darray_u32, u32, grow);

    for (u32 i = 0; i < 10; ++i) {
        assert_true(darray_u32_push(&arr, i));
    }

    // Remove from the middle
    assert_u32(darray_u32_swap_remove(&arr, 3), ==, 3);
    assert_u32(array_at(arr, 3), ==, 9);
    assert_u32(arr.len, ==, 9);

    // Remove from the end
    assert_u32(darray_u32_swap_remove(&arr, 8), ==, 8);
    assert_u32(arr.len, ==, 8);

    // Remove from the front
    assert_u32(darray_u32_swap_remove(&arr, 0), ==, 0);
    assert_u32(array_at(arr, 0), ==, 7);
    assert_u32(arr.len, ==, 7);

    darray_u32_release(&arr);
    END_TEST();
}

static void test_darray_insert(test_grow_policy_t grow) {
    BEGIN_TEST("test_darray_insert");
    SETUP_TEST(arr, darray_u32, u32, grow);

    assert_true(darray_u32_insert(&arr, 0, 1));
    assert_true(darray_u32_push(&arr, 2));
    assert_true(darray_u32_insert(&arr, 2, 3));
    assert_true(darray_u32_insert(&arr, 0, 5));

    assert_u32(array_at(arr, 0), ==, 5);
    assert_u32(array_at(arr, 1), ==, 1);
    assert_u32(array_at(arr, 2), ==, 2);
    assert_u32(array_at(arr, 3), ==, 3);

    u32 many[] = { 9, 8 };
    assert_true(darray_u32_insert_many(&arr, 1, many, 2));
    assert_u32(array_at(arr, 0), ==, 5);
    assert_u32(array_at(arr, 1), ==, 9);
    assert_u32(array_at(arr, 1), ==, 8);
    assert_u32(array_at(arr, 1), ==, 1);
    assert_u32(array_at(arr, 2), ==, 2);
    assert_u32(array_at(arr, 3), ==, 3);

    assert_true(darray_u32_insert_many(&arr, 0, many, 0));
    assert_size(arr.len, ==, 6);
    assert_u32(array_at(arr, 0), ==, 5);

    darray_u32_release(&arr);
    END_TEST();
}

static void test_darray_growing(test_grow_policy_t grow) {
    BEGIN_TEST("test_darray_grow");
    SETUP_TEST(arr, darray_u8, u8, grow);

    if (grow.kind == GROW_POLICY_FIXED) {
        assert_false(darray_u8_ensure_capacity(&arr, grow.size + 1));
    }
    assert_true(darray_u8_push_many(&arr, "abcd", 4));
    darray_u8_shrink_to_fit(&arr);

    assert_true(darray_u8_push_many(&arr, "efgh", 4));
    assert_equal_str(arr, cstr("abcdefgh"));
    darray_u8_shrink_to_fit(&arr);

    assert_true(darray_u8_insert_many(&arr, 4, "ijkl", 4));
    assert_equal_str(arr, cstr("abcdijklefgh"));

    darray_u8_release(&arr);
    END_TEST();
}

static void test_darray_stack(test_grow_policy_t grow) {
    BEGIN_TEST("test_darray_stack");
    SETUP_TEST(arr, darray_u32, u32, grow);

    assert_true(darray_u32_push(&arr, 0));
    assert_true(darray_u32_push(&arr, 1));
    assert_true(darray_u32_push(&arr, 2));
    assert_size(arr.len, ==, 3);

    for (u32 i = arr.len; i-- > 0;) {
        u32 peek, pop;
        assert_true(darray_u32_back(&arr, &peek));
        assert_true(darray_u32_pop(&arr, &pop));
        assert_u32(peek, ==, pop);
        assert_u32(pop, ==, i);
    }
    assert_false(darray_u32_back(&arr, NULL));
    assert_false(darray_u32_pop(&arr, NULL));

    darray_u32_release(&arr);
    END_TEST();
}

// FIXME: The arena is not getting reset between tests
static void test_darray(void) {
    u32 fixed_buf[MAX_CAPACITY] = { 0 };
    test_grow_policy_t grow_fixed = {
        .buffer = fixed_buf,
        .size = sizeof(fixed_buf),
        .kind = GROW_POLICY_FIXED,
    };
    u32 arena_buf[MAX_CAPACITY] = { 0 };
    arena_t arena = arena_init_fixed(arena_buf, sizeof(arena_buf));
    test_grow_policy_t grow_arena = {
        .arena = &arena,
        .kind = GROW_POLICY_ARENA,
    };
    allocator_t alloc;
    test_grow_policy_t grow_alloc = {
        .alloc = &alloc,
        .kind = GROW_POLICY_ALLOC,
    };

    test_darray_basic(grow_fixed);
    test_darray_basic(grow_arena);
    test_darray_basic(grow_alloc);
    test_darray_growing(grow_fixed);
    test_darray_growing(grow_arena);
    test_darray_growing(grow_alloc);
    test_darray_stack(grow_fixed);
    test_darray_stack(grow_arena);
    test_darray_stack(grow_alloc);
    test_darray_insert(grow_fixed);
    test_darray_insert(grow_arena);
    test_darray_insert(grow_alloc);
    test_darray_ordered_remove(grow_fixed);
    test_darray_ordered_remove(grow_arena);
    test_darray_ordered_remove(grow_alloc);
    test_darray_swap_remove(grow_fixed);
    test_darray_swap_remove(grow_arena);
    test_darray_swap_remove(grow_alloc);
}
