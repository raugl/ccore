#include "common.h"
#include <stdlib.h>
#include "hash.h"

#if defined(_WIN32)
    #include <io.h>
    #define isatty _isatty
    #define fileno _fileno
    #include <windows.h>
#endif

#define GENERICS_IMPLEMENTATION
#include "darray.h"
#include "hashmap.h"
#include "slice.h"

// =================================================================================================
// Section: Generic instantiations
// =================================================================================================

SLICE_IMPL(u8);
SLICE_IMPL(u16);
SLICE_IMPL(u32);
SLICE_IMPL(u64);

SLICE_IMPL(i8);
SLICE_IMPL(i16);
SLICE_IMPL(i32);
SLICE_IMPL(i64);

SLICE_IMPL(f32);
SLICE_IMPL(f64);
SLICE_IMPL(bool);
SLICE_RAW_IMPL(const char, string);

DARRAY_IMPL(u8);
DARRAY_IMPL(u16);
DARRAY_IMPL(u32);
DARRAY_IMPL(u64);

DARRAY_IMPL(i8);
DARRAY_IMPL(i16);
DARRAY_IMPL(i32);
DARRAY_IMPL(i64);

DARRAY_IMPL(f32);
DARRAY_IMPL(f64);
DARRAY_IMPL(bool);

HASHMAP_RAW_IMPL(u32, u32, map_u32, hash_bytes, equal_bytes)

// =================================================================================================
// Section: Panic and Logging
// =================================================================================================
void panic_impl() {
    fflush(stderr);
    __builtin_trap();
    abort();
}

void panic_log_impl(const char* fmt, ...) {
    if (isatty(fileno(stderr))) {
        fputs("\x1b[31m", stderr);
        fputs("[fatal]", stderr);
        fputs("\x1b[0m: ", stderr);
    } else {
        fputs("fatal: ", stderr);
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
    fflush(stderr);
    __builtin_trap();
    abort();
}

void log_impl(log_level_t level, const char* fmt, ...) {
    // TODO: switch to using 256 color codes.
    static const char* labels[] = {"[trace]", "[debug]", "[info ]", "[warn ]", "[error]"};
    static const char* colors[] = {"\x1b[1;90m", "\x1b[1;34m", "\x1b[32m", "\x1b[33m", "\x1b[31m"};

    if (isatty(fileno(stderr))) {
        fputs(colors[level], stderr);
        fputs(labels[level], stderr);
        fputs("\x1b[0m: ", stderr);
    } else {
        fputs(labels[level], stderr);
        fputs(": ", stderr);
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
