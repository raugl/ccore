CC = clang
CFLAGS = -std=c11 -O1 -Wall -Wextra -Wno-unused-function

SRC = src/hash.c src/allocator.c src/common.c
OBJ = $(SRC:.c=.o)

.PHONY: all
all: main

main: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

TEST = src/test/test_darray.c src/test/fuzz_deque.c src/test/fuzz_pqueue_old.c src/test/fuzz_pqueue.c

test:
	$(CC) $(CFLAGS) tests.c -o test && ./test

fuzz:
	$(CC) -fsanitize=fuzzer fuzz.c -o fuzz && ./fuzz

.PHONY: clean
clean:
	rm -f $(OBJ) main test fuzz
