#pragma once
#ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#endif

#include <assert.h>
#include <limits.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// TODO: These defines should probably be namespaced
#if defined(__clang__)
#define PUSH_DIAG_IGNORE_GNU_ZERO_ARGS                                                              \
    _Pragma("clang diagnostic push")                                                                \
    _Pragma("clang diagnostic ignored \"-Wgnu-zero-variadic-macro-arguments\"")

#define PUSH_DIAG_IGNORE_GNU_AUTO_TYPE                                                              \
    _Pragma("clang diagnostic push")                                                                \
    _Pragma("clang diagnostic ignored \"-Wgnu-auto-type\"")

#define PUSH_DIAG_IGNORE_GNU_ALIGNOF_EXPRESSION                                                     \
    _Pragma("clang diagnostic push")                                                                \
    _Pragma("clang diagnostic ignored \"-Wgnu-alignof-expression\"")

#define POP_DIAG_IGNORE _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)
#define PUSH_DIAG_IGNORE_GNU_ZERO_ARGS                                                              \
    _Pragma("GCC diagnostic push")                                                                  \
    _Pragma("GCC diagnostic ignored \"-Wpedantic\"") /* closest equivalent */

#define PUSH_DIAG_IGNORE_GNU_AUTO_TYPE                                                              \
    _Pragma("GCC diagnostic push")

#define PUSH_DIAG_IGNORE_GNU_ALIGNOF_EXPRESSION                                                     \
    _Pragma("GCC diagnostic push")                                                                  \
    _Pragma("GCC diagnostic ignored \"-Wpedantic\"") /* closest equivalent */

#define POP_DIAG_IGNORE _Pragma("GCC diagnostic pop")

#else
#define PUSH_DIAG_IGNORE_GNU_ZERO_ARGS
#define PUSH_DIAG_IGNORE_GNU_AUTO_TYPE
#define PUSH_DIAG_IGNORE_GNU_ALIGNOF_EXPRESSION
#define POP_DIAG_IGNORE
#endif

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef ptrdiff_t isize;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usize;

typedef float f32;
typedef double f64;
typedef long double f80;
typedef __uint128_t u128;

typedef const char* cstring;

// Useful marker annotations around mostly pointers
#define OUT
#define NULLABLE
#define NORETURN                           __attribute__((noreturn))
#define PACKED                             __attribute__((__packed__))
#define SCANF_FORMAT(fmt_idx, vaargs_idx)  __attribute__((format(scanf, fmt_idx, vaargs_idx)))
#define PRINTF_FORMAT(fmt_idx, vaargs_idx) __attribute__((format(printf, fmt_idx, vaargs_idx)))
#define FORCE_INLINE                       static inline __attribute__((__always_inline__))

PUSH_DIAG_IGNORE_GNU_ZERO_ARGS
// NOTE: This depends on a gcc/clang extension for macro "overloading" based on the length of
// `__VA_ARGS__`. The only other option would be to use `__VA_OPT__(,)` from C23
#define INVOKE(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define panic(...)                                                                                                                                                                                        \
    INVOKE(0, ##__VA_ARGS__, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_impl)( \
        __VA_ARGS__                                                                                                                                                                                       \
    )
POP_DIAG_IGNORE

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#define unreachable()  __builtin_unreachable()
#define bit_cast(T, x) (*(T*)(&(x)))
#define alignof_expr(expr)                                                                          \
    (__extension__({                                                                                \
        PUSH_DIAG_IGNORE_GNU_ALIGNOF_EXPRESSION                                                     \
        usize ccore_alignof_expr__ = _Alignof(expr);                                                \
        POP_DIAG_IGNORE                                                                             \
        ccore_alignof_expr__;                                                                       \
    }))

#define memset_undefined(ptr, len) memset((ptr), 0xAA, (len))
#define memset_destroyed(ptr, len) memset((ptr), 0xDE, (len))

#define log_trace(...) log_impl(LOG_LEVEL_TRACE, __VA_ARGS__)
#define log_debug(...) log_impl(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define log_info(...)  log_impl(LOG_LEVEL_INFO, __VA_ARGS__)
#define log_warn(...)  log_impl(LOG_LEVEL_WARN, __VA_ARGS__)
#define log_error(...) log_impl(LOG_LEVEL_ERROR, __VA_ARGS__)

NORETURN void panic_impl(void);
NORETURN PRINTF_FORMAT(1, 2) void panic_log_impl(const char* fmt, ...);

typedef enum {
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level_t;

extern log_level_t ccore_log_level;

PRINTF_FORMAT(2, 3) void log_impl(log_level_t level, cstring fmt, ...);
#define array_front(self)   array_at((self), 0)
#define array_back(self)    array_at((self), (self).len - 1)
#define array_at(self, idx) (self).ptr[validate_idx((idx), (self).len)]
#define array_len(arr)      sizeof((arr)) / sizeof((arr)[0])

static inline usize validate_idx(usize idx, usize len) {
    if (idx < len) return idx;
    panic("Out of bounds access");
}
