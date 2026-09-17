#pragma once
#include <inttypes.h>

#include "common.h"
#include "random.h"
#include "allocator.h"
#include "debug_allocator.h"

// TODO: Add functions which take this in to force debug allocator failures.
// In that case also make the debug allocator field private.
typedef struct testing_context {
    arena_t         arena;
    allocator_t     allocator;
    debug_allocator _debug_alloc;
    rng_t*          rng;
} testing_context;

typedef void (*testing_proc)(testing_context);

typedef struct testing_case {
    cstring      name;
    testing_proc proc;
} testing_case;

// Makes the debug allocator of the testing context fail all allocations, reallocations, or resizes
#define testing_fail_all_alloc(test) for (                                                          \
    (test)->_debug_alloc.fail_alloc = true;                                                         \
    (test)->_debug_alloc.fail_alloc;                                                                \
    (test)->_debug_alloc.fail_alloc = false                                                         \
)

// Makes the debug allocator of the testing context fail all allocations or reallocations
#define testing_fail_alloc(test) for (                                                              \
    (test)->_debug_alloc.fail_alloc = true;                                                         \
    (test)->_debug_alloc.fail_alloc;                                                                \
    (test)->_debug_alloc.fail_alloc = false                                                         \
)

// Makes the debug allocator of the testing context fail all in-place resizes, forcing
// it to fall back to realloc.
#define testing_fail_resize(test) for (                                                             \
    (test)->_debug_alloc.fail_resize = true;                                                        \
    (test)->_debug_alloc.fail_resize;                                                               \
    (test)->_debug_alloc.fail_resize = false                                                        \
)

// Runs every test in `tests`, prints a summary, returns the number of failures.
u32 run_test_suite(const testing_case* tests, usize count, i32 argc, cstring argv[]);

int LLVMFuzzerTestOneInput(const u8* data, usize size);

// TODO: replace libc's assert with my own
// FIXME: Add __LINE__ __FILE__ __func__ to the error message
#define assert_type_full(prefix, suffix, T, fmt, a, op, b)                                          \
    do {                                                                                            \
        T assert_tmp_a_ = (a);                                                                      \
        T assert_tmp_b_ = (b);                                                                      \
        if (!(assert_tmp_a_ op assert_tmp_b_)) {                                                    \
            const char assert_tmp_fmt_[] = "assertion failed: %s %s %s (" prefix "%" fmt suffix     \
                " %s " prefix "%" fmt suffix ")";                                                   \
            panic(assert_tmp_fmt_, #a, #op, #b, assert_tmp_a_, #op, assert_tmp_b_);                 \
        }                                                                                           \
    } while (0)

#define assert_type(T, fmt, a, op, b) assert_type_full("", "", T, fmt, a, op, b)

#define assert_slice(fmt, a, op, b)                                                                 \
    do {                                                                                            \
        PUSH_DIAG_IGNORE_GNU_AUTO_TYPE;                                                             \
        __auto_type assert_tmp_a_ = (a);                                                            \
        __auto_type assert_tmp_b_ = (b);                                                            \
        POP_DIAG_IGNORE;                                                                            \
                                                                                                    \
        if (assert_tmp_a_.len != assert_tmp_b_.len) {                                               \
            panic(                                                                                  \
                "assertion failed: " #a ".len (%zu) != " #b ".len (%zu)",                           \
                (usize)assert_tmp_a_.len, (usize)assert_tmp_b_.len                                  \
            );                                                                                      \
        }                                                                                           \
        for (usize assert_tmp_i_ = 0; assert_tmp_i_ < assert_tmp_a_.len; ++assert_tmp_i_) {         \
            if (!(assert_tmp_a_.data[assert_tmp_i_] op assert_tmp_b_.data[assert_tmp_i_])) {        \
                panic(                                                                              \
                    "assertion failed: " #a "[%zu] %s " #b "[%zu] (%" fmt " %s %" fmt ")",          \
                    assert_tmp_i_, #op, assert_tmp_i_,                                              \
                    assert_tmp_a_.data[assert_tmp_i_], #op, assert_tmp_b_.data[assert_tmp_i_]       \
                );                                                                                  \
            }                                                                                       \
        }                                                                                           \
    } while (0)

#define assert_str(a, b) assert_slice("c", (a), ==, (b))

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

typedef struct fuzz_reader {
    const u8* data;
    usize     index, len;
} fuzz_reader;

FORCE_INLINE bool fuzz_reader_is_empty(fuzz_reader reader) {
    return reader.index == reader.len;
}

FORCE_INLINE void fuzz_reader_impl(fuzz_reader* reader, void* dest, usize size) {
    usize avail = min_usize(reader->len - reader->index, size);
    memcpy(dest, reader->data + reader->index, avail);
    memset((u8*)dest + avail, 0, size - avail);
    reader->index += avail;
}

// NOTE: This consumes a full byte instead of the purely necessary bit
FORCE_INLINE bool fuzz_read_bool(fuzz_reader* reader) {
    bool result;
    fuzz_reader_impl(reader, &result, sizeof(u8));
    return (result & 1) == 1;
}

FORCE_INLINE u8 fuzz_read_u8(fuzz_reader* reader) {
    u8 result;
    fuzz_reader_impl(reader, &result, sizeof(u8));
    return result;
}

FORCE_INLINE u16 fuzz_read_u16(fuzz_reader* reader) {
    u16 result;
    fuzz_reader_impl(reader, &result, sizeof(u16));
    return result;
}

FORCE_INLINE u32 fuzz_read_u32(fuzz_reader* reader) {
    u32 result;
    fuzz_reader_impl(reader, &result, sizeof(u32));
    return result;
}

FORCE_INLINE u64 fuzz_read_u64(fuzz_reader* reader) {
    u64 result;
    fuzz_reader_impl(reader, &result, sizeof(u64));
    return result;
}

FORCE_INLINE usize fuzz_read_usize(fuzz_reader* reader) {
    usize result;
    fuzz_reader_impl(reader, &result, sizeof(usize));
    return result;
}

FORCE_INLINE i8 fuzz_read_i8(fuzz_reader* reader) {
    i8 result;
    fuzz_reader_impl(reader, &result, sizeof(i8));
    return result;
}

FORCE_INLINE i16 fuzz_read_i16(fuzz_reader* reader) {
    i16 result;
    fuzz_reader_impl(reader, &result, sizeof(i16));
    return result;
}

FORCE_INLINE i32 fuzz_read_i32(fuzz_reader* reader) {
    i32 result;
    fuzz_reader_impl(reader, &result, sizeof(i32));
    return result;
}

FORCE_INLINE i64 fuzz_read_i64(fuzz_reader* reader) {
    i64 result;
    fuzz_reader_impl(reader, &result, sizeof(i64));
    return result;
}

FORCE_INLINE isize fuzz_read_isize(fuzz_reader* reader) {
    isize result;
    fuzz_reader_impl(reader, &result, sizeof(isize));
    return result;
}
