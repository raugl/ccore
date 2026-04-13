#include "common.h"

// =================================================================================================
// Section: Generic instantiations
// =================================================================================================
#define GENERICS_IMPLEMENTATION

#include "slice.h"
SLICE_IMPL(u8, u8);
SLICE_IMPL(u16, u16);
SLICE_IMPL(u32, u32);
SLICE_IMPL(u64, u64);

SLICE_IMPL(i8, i8);
SLICE_IMPL(i16, i16);
SLICE_IMPL(i32, i32);
SLICE_IMPL(i64, i64);

#include "darray.h"
DARRAY_IMPL(u8, u8);
DARRAY_IMPL(u16, u16);
DARRAY_IMPL(u32, u32);
DARRAY_IMPL(u64, u64);

DARRAY_IMPL(i8, i8);
DARRAY_IMPL(i16, i16);
DARRAY_IMPL(i32, i32);
DARRAY_IMPL(i64, i64);

#include "hashmap.h"
HASHMAP_IMPL(u32, u32, u32_u32)

// =================================================================================================
// Section: Panic and Logging
// =================================================================================================
void panic_impl() {
    fflush(stderr);
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
    abort();
}

void log_impl(log_level_t level, const char* fmt, ...) {
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
