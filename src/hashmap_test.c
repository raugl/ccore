#include "common.h"
#include "slice.h"
#include "testing.h"

#define GENERICS_IMPLEMENTATION
#include "hashmap.h"

// #define MUNIT_ENABLE_ASSERT_ALIASES
#include "../vendor/munit/munit.h"

void basic_usage(void) {
    //     allocator_t alloc = {};
    //     map_u32_t map = map_u32_init_alloc(&alloc, 0);
    // cleanup:
    //     map_u32_release(&map);

    HASHMAP_ARRAY(buffer, u32, u32, 8) = {0};
    map_u32_t map = map_u32_init_fixed(buffer, 8);

    const u32 count = 5;
    u32 total = 0;

    for (u32 i = 0; i < count; i++) {
        map_u32_put(&map, i, i);
        total += i;
    }

    u32 sum = 0;
    map_iter_t it = map_u32_iter(map);
    while (map_iter_next(&it)) {
        sum += *(u32*)it.key;
    }
    assert_u32(sum, ==, total);

    sum = 0;
    for (u32 i = 0; i < count; i++) {
        u32* val = map_u32_get(&map, i);
        assert_ptr(val, !=, NULL);
        assert_u32(*val, ==, i);

        sum += *map_u32_get(&map, i);
    }
    assert_u32(sum, ==, total);
}

// HASHMAP_RAW_DECL(u64, string_t, map);
// HASHMAP_RAW_IMPL(u64, string_t, map, hash_bytes, equal_bytes);

int main(void) {
    // HASHMAP_ARRAY(buffer, u64, string_t, 1024) = {0};
    // map_t map = map_init_fixed(buffer, 1024);

    //     allocator_t alloc = {};
    //     map_t map = map_init_alloc(&alloc, 0);
    //
    //     map_put(&map, 42, cstr("yoooo"));
    //     map_put(&map, 43, cstr("yoooo"));
    //     map_put(&map, 44, cstr("yoooo"));
    //     map_put(&map, 45, cstr("yoooo"));
    //     map_put(&map, 69, cstr("nice"));
    //     map_put(&map, 70, cstr("nice"));
    //     map_put(&map, 71, cstr("nice"));
    //     map_put(&map, 72, cstr("nice"));
    //
    //     map_iter_t iter = map_iter(map);
    //     while (map_iter_next(&iter)) {
    //         u64 key = *(u64*)iter.key;
    //         string_t value = *(string_t*)iter.value;
    //         log_info("%zu: \"%.*s\"", key, (int)value.len, value.ptr);
    //     }
    //
    //     // for (usize i = 0; i < 4; ++i) {
    //     //     string_t* val = map_get(&map, 42 + i);
    //     //     assert_ptr(val, !=, NULL);
    //     //     log_info("%zu: \"%.*s\"", 42 + i, (int)val->len, val->ptr);
    //     // }
    //     // for (usize i = 0; i < 4; ++i) {
    //     //     string_t* val = map_get(&map, 69 + i);
    //     //     assert_ptr(val, !=, NULL);
    //     //     log_info("%zu: \"%.*s\"", 69 + i, (int)val->len, val->ptr);
    //     // }
    //
    // cleanup:
    //     map_release(&map);
}
