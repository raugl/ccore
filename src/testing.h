#pragma once
#include <inttypes.h>
#include "common.h"

#define assert_type_full(prefix, suffix, T, fmt, a, op, b)                                         \
    do {                                                                                           \
        T munit_tmp_a_ = (a);                                                                      \
        T munit_tmp_b_ = (b);                                                                      \
        if (!(munit_tmp_a_ op munit_tmp_b_)) {                                                     \
            const char munit_tmp_fmt_[] = "assertion failed: %s %s %s (" prefix "%" fmt suffix     \
                                          " %s " prefix "%" fmt suffix ")";                        \
            log_error(munit_tmp_fmt_, #a, #op, #b, munit_tmp_a_, #op, munit_tmp_b_);               \
        }                                                                                          \
        MUNIT_PUSH_DISABLE_MSVC_C4127_                                                             \
    } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#define assert_type(T, fmt, a, op, b) assert_type_full("", "", T, fmt, a, op, b)

#define assert_i8(a, op, b)  assert_type(i8, PRIi8, a, op, b)
#define assert_i16(a, op, b) assert_type(i16, PRIi16, a, op, b)
#define assert_i32(a, op, b) assert_type(i32, PRIi32, a, op, b)
#define assert_i64(a, op, b) assert_type(i64, PRIi64, a, op, b)
#define assert_u8(a, op, b)  assert_type(u8, PRIu8, a, op, b)
#define assert_u16(a, op, b) assert_type(u16, PRIu16, a, op, b)
#define assert_u32(a, op, b) assert_type(u32, PRIu32, a, op, b)
#define assert_u64(a, op, b) assert_type(u64, PRIu64, a, op, b)

#define assert_size(a, op, b) assert_type(usize, MUNIT_SIZE_MODIFIER "u", a, op, b)

#define assert_f32(a, op, b) assert_type(f32, "f", a, op, b)
#define assert_f64(a, op, b) assert_type(f64, "g", a, op, b)
#define assert_ptr(a, op, b) assert_type(const void*, "p", a, op, b)
