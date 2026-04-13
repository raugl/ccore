#pragma once
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

// Useful marker annotations around mostly pointers
#define OUT
#define NULLABLE
#define INVOKE(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

#define unreachable __builtin_trap()

// NOTE: This depends on a gcc/clang extension for macro "overloading" based on the length of
// `__VA_ARGS__`. The only other option would be to use `__VA_OPT(,)` from C23
#define panic(...)                                                                                                                                                                                        \
    INVOKE(0, ##__VA_ARGS__, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_log_impl, panic_impl)( \
        __VA_ARGS__                                                                                                                                                                                       \
    )

#define at(items, idx)                                                                             \
    ((idx) < ((items).len)) ? (items).ptr[idx] : (panic("Out of bounds access"), (items).ptr[0])

#define for_each(type, item, list)                                                                 \
    for (type* item = (list).ptr; item < (list).ptr + (list).len; ++item)

#define log_trace(...) log_impl(LOG_LEVEL_TRACE, __VA_ARGS__)
#define log_debug(...) log_impl(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define log_info(...)  log_impl(LOG_LEVEL_INFO, __VA_ARGS__)
#define log_warn(...)  log_impl(LOG_LEVEL_WARN, __VA_ARGS__)
#define log_error(...) log_impl(LOG_LEVEL_ERROR, __VA_ARGS__)

__attribute__((noreturn)) void panic_impl();
__attribute__((noreturn)) __attribute__((format(printf, 1, 2))) void
panic_log_impl(const char* fmt, ...);

typedef enum {
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level_t;

__attribute__((format(printf, 2, 3))) void log_impl(log_level_t level, const char* fmt, ...);

static inline uintptr_t align_forward(uintptr_t address, uintptr_t align) {
    return (address + (align - 1)) & ~(align - 1);
}

static inline void* align_forward_ptr(void* ptr, uintptr_t align) {
    return (void*)(((uintptr_t)ptr + (align - 1)) & ~(align - 1));
}

// TODO:
// #define assert_int(lhs, op, rhs) \
//   do { \
//     if (!((lhs)(op)(rhs))) { \
//       fprintf(stderr, "failed assertion '%s %s %s': '%d %s %d'", ##lhs, ##op,
//       \
//               ##rhs, (lhs), ##op, (rhs)); \
//       __builtin_trap(); \
//     } \
//   } while (false)
