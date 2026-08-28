#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include <core/common.h>
#include <core/hash.h>
#include <core/darray.h>
#include <core/allocator.h>
#include <core/testing.h>

#define PIPE_READ  0
#define PIPE_WRITE 1

typedef enum test_status {
    TEST_PASSED,
    TEST_FAILED,
    TEST_TIMED_OUT,
} test_status;

typedef struct test_run_result {
    int        signal;      // 0 if exited normally
    int        exit_code;   // valid if !timed_out && signal == 0
    test_status status;
} test_result;

// Drain a pipe into `out` until EOF. Used by the parent after the child exits/is killed,
// so we don't deadlock waiting on a full pipe buffer while the child is still writing.
static void drain_pipe(int fd, darray_u8* output) {
    u8 buf[4096];
    while (true) {
        isize n = read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        if (!darray_u8_push_many(output, buf, (usize)n)) panic("Test runner ran out of memory");
    }
}

// Runs a single test in a forked child. `timeout_seconds` == 0 disables the timeout.
static test_result run_test_case(testing_case test, u64 seed, u32 timeout_seconds, darray_u8* output) {
    test_result result = { 0 };

    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        panic("pipe() failed: %s", strerror(errno));
    }

    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();
    if (pid < 0) {
        panic("fork() failed: %s", strerror(errno));
    }

    // Chile process
    if (pid == 0) {
        close(pipe_fds[PIPE_READ]);
        dup2(pipe_fds[PIPE_WRITE], STDOUT_FILENO);
        dup2(pipe_fds[PIPE_WRITE], STDERR_FILENO);
        close(pipe_fds[PIPE_WRITE]);

        if (timeout_seconds > 0) {
            alarm((u32)timeout_seconds); // default disposition (SIGALRM) kills the process
        }

        rng_t rng = rng_seed(seed);
        allocator_t gpa = allocator_init_malloc();

        testing_context ctx = {
            .rng          = &rng,
            .arena        = arena_init_alloc(gpa, 32 * 1024),
            ._debug_alloc = debug_allocator_init(gpa, gpa),
        };
        ctx.allocator = allocator_init_debug(&ctx._debug_alloc);

        assert(test.proc != NULL && "Did you forget to asign the test's function pointer?");
        test.proc(ctx);

        bool leaked = debug_allocator_release(&ctx._debug_alloc, true);
        arena_release(&ctx.arena);
        _exit(leaked);
    }

    // Parent process
    close(pipe_fds[PIPE_WRITE]);

    int status = 0;
    pid_t waited = waitpid(pid, &status, 0);
    if (waited < 0) {
        panic("waitpid() failed: %s", strerror(errno));
    }

    // NOTE: The pipe is only drained once the test has finished. This should be fine for tests
    // which print a reasonable amount, but if I ever overflow the default size of the buffer, I
    // think that I will deadlock
    drain_pipe(pipe_fds[PIPE_READ], output);
    close(pipe_fds[PIPE_READ]);

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
        result.status    = (result.exit_code == 0) ? TEST_PASSED : TEST_FAILED;
    } else if (WIFSIGNALED(status)) {
        result.signal    = WTERMSIG(status);
        result.status    = (result.signal == SIGALRM) ? TEST_TIMED_OUT : TEST_FAILED;
    } else {
        // Stopped/continued — shouldn't happen with a plain waitpid(), treat as failure.
        result.status    = TEST_FAILED;
    }
    return result;
}

static void print_usage(cstring prog) {
    fprintf(stderr,
        "usage: %s [--filter=<prefix>] [--seed=<u64>] [--timeout=<seconds>]\n"
        "  --filter=<prefix>   only run tests whose name starts with <prefix> (default: run all)\n"
        "  --seed=<u64>        RNG seed to use for every test (default: random)\n"
        "  --timeout=<seconds> per-test timeout in seconds, 0 disables it (default: 5)\n",
        prog);
}

// Parses "--name=value" or "--name value". Returns the value pointer (into argv, not owned)
// or NULL if `arg` doesn't match `name`. Advances *i past a separate-token value.
static cstring match_arg(cstring arg, cstring name, i32 argc, cstring argv[], i32* i) {
    usize name_len = strlen(name);
    if (strncmp(arg, name, name_len) != 0) return NULL;

    if (arg[name_len] == '=') return arg + name_len + 1;
    if (arg[name_len] == '\0') {
        if (*i + 1 >= argc) {
            fprintf(stderr, "error: %s requires a value\n", name);
            exit(EXIT_FAILURE);
        }
        return argv[++*i];
    }
    return NULL; // e.g. "--seedfoo" shouldn't match "--seed"
}

static u64 parse_u64_arg(cstring value, cstring name) {
    char* end;
    u64 result = strtoull(value, &end, 10);
    if (end == value || *end != '\0') {
        fprintf(stderr, "error: invalid value for %s: '%s'\n", name, value);
        exit(EXIT_FAILURE);
    }
    return result;
}

u32 run_test_suite(const testing_case* tests, usize count, i32 argc, cstring argv[]) {
    cstring filter  = NULL;
    rng_t   seed    = rng_seed_entropy();
    u32     timeout = 5; // in seconds

    for (i32 i = 1; i < argc; ++i) {
        cstring arg = argv[i];
        cstring value;

        if ((value = match_arg(arg, "--filter", argc, argv, &i))) {
            filter = value;
        } else if ((value = match_arg(arg, "--seed", argc, argv, &i))) {
            seed = parse_u64_arg(value, "--seed");
        } else if ((value = match_arg(arg, "--timeout", argc, argv, &i))) {
            timeout = (u32)parse_u64_arg(value, "--timeout");
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "error: unrecognized argument '%s'\n", arg);
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    u32 failures = 0;
    FILE* stream = stderr;
    allocator_t gpa = allocator_init_malloc();
    darray_u8 output = darray_u8_init_alloc(gpa);

    for (usize i = 0; i < count; ++i) {
        if (filter && !cstring_starts_with(tests[i].name, filter)) continue;

        darray_u8_clear(&output);
        test_result result = run_test_case(tests[i], seed, timeout, &output);

        if (result.status == TEST_PASSED) {
            fprintf_color(stream, ANSI_GREEN, "[PASS]");
            fprintf_color(stream, ANSI_WHITE, " '%s'\n", tests[i].name ? tests[i].name : "unnamed test");
        } else {
            failures++;
            fprintf_color(stream, ANSI_RED, "[FAIL]");
            fprintf_color(stream, ANSI_WHITE, " '%s'", tests[i].name ? tests[i].name : "unnamed test");

            if (result.status == TEST_TIMED_OUT) {
                fprintf_color(
                    stream, ANSI_BRIGHT_BLACK, " (timed out after %ds, seed=%lu)\n",
                    timeout, seed
                );
            } else if (result.signal != 0) {
                fprintf_color(
                    stream, ANSI_BRIGHT_BLACK, " (killed by signal %d: %s, seed=%lu)\n",
                    result.signal, strsignal(result.signal), seed
                );
            } else {
                fprintf_color(
                    stream, ANSI_BRIGHT_BLACK, " (exit code %d, seed=%lu)\n",
                    result.exit_code, seed
                );
            }

            if (output.len > 0) {
                fwrite(output.ptr, 1, output.len, stream);
                if (output.ptr[output.len - 1] != '\n') putc('\n', stream);
            }
        }
    }

    // FIXME: I'd rather this was printed just once at the end. Now I need to make this function
    // rentrant
    printf("\nRan %zu tests, %u failed\n", count, failures);
    darray_u8_release(&output);
    return failures;
}

// TODO: Parellel test runner example, dig in
// typedef struct running_test {
//     pid_t pid;
//     int   pipe_fd;
//     usize test_idx;
//     u64   seed;
//     darray_u8 output;
//     struct timespec started_at;
// } running_test;
//
// u32 run_test_suite_parallel(const test_case* tests, usize count, i32 timeout_seconds, u32 max_parallel) {
//     allocator_t heap = allocator_init_malloc();
//     running_test* slots = mem_alloc(heap, running_test, max_parallel);
//     memset(slots, 0, max_parallel * sizeof(running_test));
//
//     usize next_test = 0;
//     u32   in_flight = 0;
//     u32   failures  = 0;
//
//     while (next_test < count || in_flight > 0) {
//         // fill any free slots
//         for (u32 i = 0; i < max_parallel && next_test < count; ++i) {
//             if (slots[i].pid != 0) continue;
//
//             u64 seed = hash_mix((u64)next_test, 0xC0FFEE);
//             slots[i] = spawn_test(tests[next_test], seed, heap); // fork()s, doesn't wait
//             slots[i].test_idx = next_test;
//             clock_gettime(CLOCK_MONOTONIC, &slots[i].started_at);
//             next_test++;
//             in_flight++;
//         }
//
//         struct pollfd fds[64]; // == max_parallel, bounded for the example
//         u32 fd_count = 0;
//         for (u32 i = 0; i < max_parallel; ++i) {
//             if (slots[i].pid != 0) fds[fd_count++] = (struct pollfd){ .fd = slots[i].pipe_fd, .events = POLLIN };
//         }
//
//         poll(fds, fd_count, 200 /* ms, also our timeout-check granularity */);
//
//         for (u32 i = 0; i < max_parallel; ++i) {
//             if (slots[i].pid == 0) continue;
//
//             // drain whatever's available without blocking
//             u8 buf[4096];
//             isize n;
//             while ((n = read(slots[i].pipe_fd, buf, sizeof(buf))) > 0) {
//                 darray_u8_push_many(&slots[i].output, buf, (usize)n);
//             }
//
//             int status;
//             pid_t r = waitpid(slots[i].pid, &status, WNOHANG);
//             bool timed_out = elapsed_seconds(slots[i].started_at) > (f64)timeout_seconds;
//
//             if (r == 0 && timed_out) {
//                 kill(slots[i].pid, SIGKILL);
//                 waitpid(slots[i].pid, &status, 0);
//                 r = slots[i].pid;
//             }
//             if (r == 0) continue; // still running, not timed out
//
//             report_result(tests[slots[i].test_idx].name, status, slots[i].seed, &slots[i].output, &failures);
//
//             close(slots[i].pipe_fd);
//             darray_u8_release(&slots[i].output);
//             slots[i] = (running_test){ 0 };
//             in_flight--;
//         }
//     }
//
//     mem_free(heap, slots, max_parallel);
//     return failures;
// }
