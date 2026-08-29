#define GENERICS_IMPLEMENTATION
#include <core/common.h>
#include <core/darray.h>
#include <core/slice.h>
#include <core/testing.h>

DARRAY_DECL(i32)
DARRAY_IMPL(i32)

static void test_darray_basic(testing_context test) {
    darray_u32 arr = darray_u32_init_alloc(test.allocator);
    for (u32 i = 0; i < 10; ++i) {
        assert_true(darray_u32_append(&arr, i + 1));
    }
    for (u32 i = 0; i < 10; ++i) {
        assert_u32(array_at(arr, i), ==, i + 1);
    }

    u32 actual;
    assert_true(darray_u32_pop(&arr, &actual)); assert_u32(actual, ==, 10);
    assert_size(arr.len, ==, 9);

    assert_true(darray_u32_append_many(&arr, (u32[]) { 1, 2, 3 }, 3));
    assert_size(arr.len, ==, 12);

    assert_true(darray_u32_pop(&arr, &actual)); assert_u32(actual, ==, 3);
    assert_true(darray_u32_pop(&arr, &actual)); assert_u32(actual, ==, 2);
    assert_true(darray_u32_pop(&arr, &actual)); assert_u32(actual, ==, 1);
    assert_size(arr.len, ==, 9);

    assert_true(darray_u32_append_many(&arr, NULL, 0));
    assert_size(arr.len, ==, 9);

    array_at(arr, 7) = 69;
    array_at(arr, 8) = 42;
    assert_true(darray_u32_pop(&arr, &actual)); assert_u32(actual, ==, 42);
    assert_true(darray_u32_pop(&arr, &actual)); assert_u32(actual, ==, 69);

    u32 many[4]; // NOTE: This should be too small, I want to see this test failing
    testing_fail_next_alloc(&test);
    assert_false(darray_u32_reserve_spare(&arr, array_len(many)));
    assert_false(darray_u32_append_many(&arr, many, array_len(many)));
    assert_false(darray_u32_insert_many(&arr, 4, many, array_len(many)));
    darray_u32_destroy(&arr);
}

static void test_darray_ordered_remove(testing_context test) {
    {
        darray_u32 arr = darray_u32_init_alloc(test.allocator);
        for (u32 i = 0; i < 10; ++i) {
            assert_true(darray_u32_append(&arr, i + 1));
        }
        // Remove from the middle
        assert_u32(darray_u32_remove(&arr, 3), ==, 4);
        assert_u32(array_at(arr, 3), ==, 5);
        assert_size(arr.len, ==, 9);

        // Remove from the end
        assert_u32(darray_u32_remove(&arr, 8), ==, 10);
        assert_size(arr.len, ==, 8);

        // Remove from the front
        assert_u32(darray_u32_remove(&arr, 0), ==, 1);
        assert_u32(array_at(arr, 0), ==, 2);
        assert_size(arr.len, ==, 7);
        darray_u32_destroy(&arr);
    }
    {
        // remove last item
        darray_u32 arr = darray_u32_init_alloc(test.allocator);
        assert_true(darray_u32_append(&arr, 1));
        assert_u32(1, ==, darray_u32_remove(&arr, 0));
        assert_size(arr.len, ==, 0);
        darray_u32_destroy(&arr);
    }
    {
        darray_u32 arr = darray_u32_init_alloc(test.allocator);
        for (u32 i = 0; i < 10; ++i) {
            assert_true(darray_u32_append(&arr, i + 1));
        }
        {
            const u32 expected[] = { 1, 2, 3, 7, 8, 9, 10 };
            darray_u32_remove_many(&arr, 3, 3);

            assert_size(arr.len, ==, array_len(expected));
            for (u32 i = 0; i < arr.len; ++i) {
                assert_u32(arr.data[i], ==, expected[i]);
            }
        }
        {
            const u32 expected[] = { 1, 2, 3, 7, 8 };
            darray_u32_remove_many(&arr, 5, 2);

            assert_size(arr.len, ==, array_len(expected));
            for (u32 i = 0; i < arr.len; ++i) {
                assert_u32(arr.data[i], ==, expected[i]);
            }
        }
        {
            const u32 expected[] = { 7, 8 };
            darray_u32_remove_many(&arr, 0, 3);

            assert_size(arr.len, ==, array_len(expected));
            for (u32 i = 0; i < arr.len; ++i) {
                assert_u32(arr.data[i], ==, expected[i]);
            }
        }
        darray_u32_destroy(&arr);
    }
}

static void test_darray_swap_remove(testing_context test) {
    darray_u32 arr = darray_u32_init_alloc(test.allocator);
    for (u32 i = 0; i < 10; ++i) {
        assert_true(darray_u32_append(&arr, i + 1));
    }

    // Remove from the middle
    assert_u32(darray_u32_swap_remove(&arr, 3), ==, 4);
    assert_u32(array_at(arr, 3), ==, 10);
    assert_size(arr.len, ==, 9);

    // Remove from the end
    assert_u32(darray_u32_swap_remove(&arr, 8), ==, 9);
    assert_size(arr.len, ==, 8);

    // Remove from the front
    assert_u32(darray_u32_swap_remove(&arr, 0), ==, 1);
    assert_u32(array_at(arr, 0), ==, 8);
    assert_size(arr.len, ==, 7);
    darray_u32_destroy(&arr);
}

static void test_darray_insert(testing_context test) {
    {
        darray_u32 arr = darray_u32_init_alloc(test.allocator);

        assert_true(darray_u32_insert(&arr, 0, 1));
        assert_true(darray_u32_append(&arr, 2));
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
        assert_u32(array_at(arr, 2), ==, 8);
        assert_u32(array_at(arr, 3), ==, 1);
        assert_u32(array_at(arr, 4), ==, 2);
        assert_u32(array_at(arr, 5), ==, 3);

        assert_true(darray_u32_insert_many(&arr, 0, NULL, 0));
        assert_size(arr.len, ==, 6);
        assert_u32(array_at(arr, 0), ==, 5);
        darray_u32_destroy(&arr);
    }
    {
        u8 buffer[10];
        darray_u8 arr = darray_u8_init_fixed(buffer, array_len(buffer));

        assert_true(darray_u8_append_many(&arr, "abcd", 4));
        assert_true(darray_u8_insert_many(&arr, 2, "ef", 2));
        assert_str(arr, cstr("abefcd"));

        assert_true(darray_u8_insert_many(&arr, 4, "gh", 2));
        assert_str(arr, cstr("abefghcd"));

        assert_false(darray_u8_insert_many(&arr, 6, "ijkl", 4));
        assert_str(arr, cstr("abefghcd")); // ensure no elements were changed before erroring out

        assert_true(darray_u8_insert_many(&arr, 6, "ij", 2));
        assert_str(arr, cstr("abefghijcd"));
    }
}

static void test_darray_shrinking(testing_context test) {
    darray_i32 arr = darray_i32_init_alloc(test.allocator);

    assert_true(darray_i32_reserve(&arr, 16));
    assert_size(arr.capacity, >=, 16);

    assert_true(darray_i32_append(&arr, 1));
    assert_true(darray_i32_append(&arr, 2));
    assert_true(darray_i32_append(&arr, 3));

    assert_true(darray_i32_shrink_to_fit(&arr));
    assert_size(arr.capacity, ==, 3);

    assert_true(darray_i32_reserve(&arr, 16));
    assert_size(arr.capacity, >=, 16);

    testing_fail_next_alloc(&test);
    assert_false(darray_i32_shrink_to_fit(&arr));
    assert_size(arr.capacity, >=, 16);
    darray_i32_destroy(&arr);
}

static void test_darray_growing(testing_context test) {
    darray_u8 arr = darray_u8_init_alloc(test.allocator);

    testing_fail_next_alloc(&test);
    assert_false(darray_u8_reserve(&arr, 42));

    assert_true(darray_u8_add_many(&arr, 0, 4));
    memcpy(arr.data, "aoeu", 4);

    assert_true(darray_u8_add_many(&arr, 0, 4));
    memcpy(arr.data, "asdf", 4);
    assert_str(arr, cstr("asdfaoeu"));

    // Shrink the list after every insertion to ensure that a memory growth
    // will be triggered in the next operation.
    assert_true(darray_u8_append_many(&arr, "abcd", 4));
    assert_true(darray_u8_shrink_to_fit(&arr));

    assert_true(darray_u8_append_many(&arr, "efgh", 4));
    assert_str(arr, cstr("abcdefgh"));
    assert_true(darray_u8_shrink_to_fit(&arr));

    assert_true(darray_u8_insert_many(&arr, 4, "ijkl", 4));
    assert_str(arr, cstr("abcdijklefgh"));
    darray_u8_destroy(&arr);
}

static void test_darray_stack(testing_context test) {
    darray_u32 arr = darray_u32_init_alloc(test.allocator);

    assert_false(darray_u32_back(&arr, NULL));
    assert_false(darray_u32_pop(&arr, NULL));

    assert_true(darray_u32_append(&arr, 0));
    assert_true(darray_u32_append(&arr, 1));
    assert_true(darray_u32_append(&arr, 2));
    assert_size(arr.len, ==, 3);

    u32 actual;
    const darray_u32 const_arr = arr;
    assert_true(darray_u32_back(&const_arr, &actual));
    assert_u32(actual, ==, 2);

    for (i32 i = (i32)arr.len - 1; i > 0; --i) {
        u32 peek, pop;
        assert_true(darray_u32_back(&arr, &peek));
        assert_true(darray_u32_pop(&arr, &pop));
        assert_u32(peek, ==, pop);
        assert_u32(pop, ==, (u32)i);
    }
    assert_false(darray_u32_back(&arr, NULL));
    assert_false(darray_u32_pop(&arr, NULL));
    darray_u32_destroy(&arr);
}

static void test_darray_int_overflow(testing_context test) {
    darray_u32 arr = {
        .len        = UINT32_MAX - 1,
        .capacity   = UINT32_MAX - 1,
        ._allocator = test.allocator,
    };
    assert_false(darray_u32_append(&arr, 42));
    assert_false(darray_u32_insert(&arr, 0, 69));
    assert_false(darray_u32_add_many(&arr, 0, 2));
    assert_false(darray_u32_append_many(&arr, (u32[]) { 1, 2 }, 2));
    assert_false(darray_u32_insert_many(&arr, 0, (u32[]) { 1, 2 }, 2));
    assert_false(darray_u32_reserve_spare(&arr, 2));

}

const testing_case test_suite_darray[] = {
    { "darray_basic",          &test_darray_basic          },
    { "darray_growing",        &test_darray_growing        },
    { "darray_stack",          &test_darray_stack          },
    { "darray_insert",         &test_darray_insert         },
    { "darray_ordered_remove", &test_darray_ordered_remove },
    { "darray_swap_remove",    &test_darray_swap_remove    },
    { "darray_int_overflow",   &test_darray_int_overflow   },
};
