#include <core/testing.h>

#include "test_deque.c"
#include "test_darray.c"
#include "test_pqueue.c"
#include "test_hashmap.c"

int main(i32 argc, cstring argv[]) {
    u32 failures = 0;

    failures += run_test_suite(test_suite_deque, array_len(test_suite_deque), argc, argv);
    failures += run_test_suite(test_suite_darray, array_len(test_suite_darray), argc, argv);
    failures += run_test_suite(test_suite_pqueue, array_len(test_suite_pqueue), argc, argv);
    failures += run_test_suite(test_suite_hashmap, array_len(test_suite_hashmap), argc, argv);

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
