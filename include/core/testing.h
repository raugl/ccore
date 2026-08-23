#pragma once
#include <inttypes.h>
#include "allocator.h"
#include "common.h"
#include "random.h"

int LLVMFuzzerTestOneInput(const u8* data, usize size);

typedef struct {
    union {
        struct {
            void* buffer;
            usize size;
        };

        arena_t* arena;
        allocator_t* alloc;
    };

    // grow_policy_kind_t kind;
} test_grow_policy_t;

// TODO:
#define BEGIN_TEST(name)
#define END_TEST()

#define SETUP_TEST(var, name, T, grow)                                                              \
    name##_t var;                                                                                   \
    switch (grow.kind) {                                                                            \
    case GROW_POLICY_FIXED:                                                                         \
        var = name##_init_fixed(grow.buffer, grow.size / sizeof(T));                                \
        break;                                                                                      \
    case GROW_POLICY_ARENA:                                                                         \
        var = name##_init_arena(grow.arena);                                                        \
        break;                                                                                      \
    case GROW_POLICY_ALLOC:                                                                         \
        var = name##_init_alloc(grow.alloc);                                                        \
        break;                                                                                      \
    }

#define assert_type_full(prefix, suffix, T, fmt, a, op, b)                                          \
    do {                                                                                            \
        T munit_tmp_a_ = (a);                                                                       \
        T munit_tmp_b_ = (b);                                                                       \
        if (!(munit_tmp_a_ op munit_tmp_b_)) {                                                      \
            const char munit_tmp_fmt_[] = "assertion failed: %s %s %s (" prefix "%" fmt suffix      \
                                          " %s " prefix "%" fmt suffix ")";                         \
            panic(munit_tmp_fmt_, #a, #op, #b, munit_tmp_a_, #op, munit_tmp_b_);                    \
        }                                                                                           \
    } while (0)

#define assert_type(T, fmt, a, op, b) assert_type_full("", "", T, fmt, a, op, b)

#define assert_i8(a, op, b)  assert_type(i8, PRIi8, a, op, b)
#define assert_i16(a, op, b) assert_type(i16, PRIi16, a, op, b)
#define assert_i32(a, op, b) assert_type(i32, PRIi32, a, op, b)
#define assert_i64(a, op, b) assert_type(i64, PRIi64, a, op, b)
#define assert_u8(a, op, b)  assert_type(u8, PRIu8, a, op, b)
#define assert_u16(a, op, b) assert_type(u16, PRIu16, a, op, b)
#define assert_u32(a, op, b) assert_type(u32, PRIu32, a, op, b)
#define assert_u64(a, op, b) assert_type(u64, PRIu64, a, op, b)

#define assert_size(a, op, b) assert_type(usize, "zu", a, op, b)
#define assert_f32(a, op, b)  assert_type(f32, "f", a, op, b)
#define assert_f64(a, op, b)  assert_type(f64, "g", a, op, b)
#define assert_ptr(a, op, b)  assert_type(const void*, "p", a, op, b)

#define assert_true(x)  assert((x))
#define assert_false(x) assert(!(x))

#define assert_equal_str(a, b)                                                                      \
    do {                                                                                            \
        DIAG_PUSH;                                                                                  \
        DIAG_IGNORE_GNU_AUTO_TYPE;                                                                  \
        __auto_type test_tmp_a_ = (a);                                                              \
        __auto_type test_tmp_b_ = (b);                                                              \
        DIAG_POP;                                                                                   \
                                                                                                    \
        if (test_tmp_a_.len != test_tmp_b_.len) {                                                   \
            panic(                                                                                  \
                "assertion failed: " #a ".len (%zu) != " #b ".len (%zu)",                           \
                (usize)test_tmp_a_.len,                                                             \
                (usize)test_tmp_b_.len                                                              \
            );                                                                                      \
        }                                                                                           \
        for (usize test_idx_ = 0; test_idx_ < test_tmp_a_.len; ++test_idx_) {                       \
            if (test_tmp_a_.ptr[test_idx_] != test_tmp_b_.ptr[test_idx_]) {                         \
                panic(                                                                              \
                    "assertion failed: " #a "[%zu] (%c) != " #b "[%zu] (%c)",                       \
                    test_idx_,                                                                      \
                    test_tmp_a_.ptr[test_idx_],                                                     \
                    test_idx_,                                                                      \
                    test_tmp_b_.ptr[test_idx_]                                                      \
                );                                                                                  \
            }                                                                                       \
        }                                                                                           \
    } while (0)

typedef struct {
    const u8* ptr;
    u32 idx, len;
} fuzz_reader_t;

static bool fuzz_reader_is_empty(fuzz_reader_t reader) {
    return reader.idx == reader.len;
}

static inline void fuzz_reader_impl(fuzz_reader_t* reader, void* dest, usize len) {
    usize avail = min_usize(reader->len - reader->idx, len);
    memcpy(dest, reader->ptr + reader->idx, avail);
    memset((u8*)dest + avail, 0, len - avail);
    reader->idx += avail;
}

static bool fuzz_read_bool(fuzz_reader_t* reader) {
    bool result;
    fuzz_reader_impl(reader, &result, sizeof(bool));
    return (result & 1) == 1;
}

static u8 fuzz_read_u8(fuzz_reader_t* reader) {
    u8 result;
    fuzz_reader_impl(reader, &result, sizeof(u8));
    return result;
}

static u16 fuzz_read_u16(fuzz_reader_t* reader) {
    u16 result;
    fuzz_reader_impl(reader, &result, sizeof(u16));
    return result;
}

static u32 fuzz_read_u32(fuzz_reader_t* reader) {
    u32 result;
    fuzz_reader_impl(reader, &result, sizeof(u32));
    return result;
}

static u64 fuzz_read_u64(fuzz_reader_t* reader) {
    u64 result;
    fuzz_reader_impl(reader, &result, sizeof(u64));
    return result;
}

static usize fuzz_read_usize(fuzz_reader_t* reader) {
    usize result;
    fuzz_reader_impl(reader, &result, sizeof(usize));
    return result;
}

static i8 fuzz_read_i8(fuzz_reader_t* reader) {
    i8 result;
    fuzz_reader_impl(reader, &result, sizeof(i8));
    return result;
}

static i16 fuzz_read_i16(fuzz_reader_t* reader) {
    i16 result;
    fuzz_reader_impl(reader, &result, sizeof(i16));
    return result;
}

static i32 fuzz_read_i32(fuzz_reader_t* reader) {
    i32 result;
    fuzz_reader_impl(reader, &result, sizeof(i32));
    return result;
}

static i64 fuzz_read_i64(fuzz_reader_t* reader) {
    i64 result;
    fuzz_reader_impl(reader, &result, sizeof(i64));
    return result;
}

static isize fuzz_read_isize(fuzz_reader_t* reader) {
    isize result;
    fuzz_reader_impl(reader, &result, sizeof(isize));
    return result;
}
