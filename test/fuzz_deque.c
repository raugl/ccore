#include "../common.h"
#include "../darray.h"
#include "../deque.h"
#include "../testing.h"

#define MAX_CAPACITY 256

typedef enum {
    ACTION_PUSH_BACK,
    ACTION_PUSH_FRONT,
    ACTION_POP_BACK,
    ACTION_POP_FRONT,
    ACTIONS_COUNT,
} action_kind_t;

static action_kind_t fuzz_read_weighted_action(fuzz_reader_t* input) {
    u8 value = fuzz_read_u8(input) & 7;
    if (value < 3) return ACTION_PUSH_BACK; // NOTE: Biased so it tends to grow over time
    if (value < 6) return ACTION_PUSH_FRONT;
    if (value < 7) return ACTION_POP_BACK;
    if (value < 8) return ACTION_POP_FRONT;
    unreachable;
}

void fuzz_deque(test_grow_policy_t grow, rng_t* rng, fuzz_reader_t input) {
    BEGIN_TEST("fuzz_deque");
    SETUP_TEST(deque, deque_u32, u32, grow);

    u32 oracle_buf[MAX_CAPACITY] = { 0 };
    darray_u32_t oracle = darray_u32_init_fixed(oracle_buf, MAX_CAPACITY);
    u32 item, actual, expected;
    bool has_act, has_exp;

    while (!fuzz_reader_is_empty(input)) {
        if (grow.kind == GROW_POLICY_ARENA && rng_range_int(rng, 0, 64) == 13) {
            arena_alloc(grow.arena, 1);
        }

        switch (fuzz_read_weighted_action(&input)) {
        case ACTION_PUSH_BACK:
            item = fuzz_read_u32(&input);
            if (darray_u32_push(&oracle, item)) {
                assert(deque_u32_push_back(&deque, item));
            }
            break;
        case ACTION_PUSH_FRONT:
            item = fuzz_read_u32(&input);
            if (darray_u32_insert(&oracle, item, 0)) {
                assert(deque_u32_push_front(&deque, item));
            }
            break;
        case ACTION_POP_BACK:
            has_act = deque_u32_pop_back(&deque, &actual);
            has_exp = darray_u32_pop(&oracle, &expected);
            assert_u8(has_act, ==, has_exp);

            if (has_act) assert_u32(actual, ==, expected);
            break;
        case ACTION_POP_FRONT:
            has_act = deque_u32_pop_front(&deque, &actual);
            has_exp = oracle.len > 0;
            assert_u8(has_act, ==, has_exp);

            if (has_act) assert_u32(actual, ==, darray_u32_remove(&oracle, 0));
            break;
        case ACTIONS_COUNT:
            unreachable;
        }

        has_act = deque_u32_front(&deque, &actual);
        has_exp = oracle.len > 0;
        assert_u8(has_act, ==, has_exp);
        if (has_act) assert_u32(actual, ==, array_front(oracle));

        has_act = deque_u32_back(&deque, &actual);
        has_exp = oracle.len > 0;
        assert_u8(has_act, ==, has_exp);
        if (has_act) assert_u32(actual, ==, array_front(oracle));

        assert_u32(deque.len, ==, oracle.len);
        for (usize idx = 0; idx < oracle.len; ++idx) {
            assert_u32(deque_at(deque, idx), ==, array_at(oracle, idx));
        }
    }

    deque_u32_release(&deque);
    END_TEST();
}

int LLVMFuzzerTestOneInput(const u8* data, usize size) {
    fuzz_reader_t input = { .ptr = data, .len = size };
    rng_t rng = rng_seed(fuzz_read_u64(&input));

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
    fuzz_deque(grow_fixed, &rng, input);
    fuzz_deque(grow_arena, &rng, input);
    fuzz_deque(grow_alloc, &rng, input);
    return EXIT_SUCCESS;
}

// typedef struct {
//     rng_t rng;
//     u32 sizes[2];
//     bool used[2];
//     u32 buffers[MAX_CAPACITY][2];
// } fuzz_allocator_t;
//
// // TODO: Use the rng to add some variety
// void* fuzz_allocator_fn(void* ctx, void* ptr, usize old_size, usize new_size) {
//     fuzz_allocator_t* self = ctx;
//
//     if (ptr == NULL && old_size == 0) { // alloc
//         if (new_size > MAX_CAPACITY * sizeof(u32)) return NULL;
//
//         u8 slot = (self->used[0]) ? 1 : 0;
//         assert(!self->used[slot]);
//
//         self->used[slot] = true;
//         self->sizes[slot] = new_size;
//         return self->buffers[slot];
//     }
//     if (ptr != NULL && new_size > old_size) { // realloc
//         return NULL;
//     }
//     if (ptr != NULL && new_size == 0) { // free
//         u8 slot = ptr == self->buffers[0];
//         assert_ptr(ptr, ==, self->buffers[1]);
//
//         self->used[slot] = false;
//         self->sizes[slot] = 0;
//         return NULL;
//     }
//     return NULL;
// }
