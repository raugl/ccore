CC := clang
OPT := -g -O1
CFLAGS := -std=c11 -MMD -MP  \
	-Wall -Wextra -Wpedantic \
	-Wshadow -Wundef -Wcast-qual \
	-Wformat=2 -Wstrict-overflow=5 \
	-Wconversion -Wsign-conversion -Wdouble-promotion \
	-Wswitch-enum -Wswitch-default -Wimplicit-fallthrough \
	-Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition \
	-Wwrite-strings -Wnull-dereference \
	-Wno-unused-function

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

BIN := $(BUILD_DIR)/main
SRC := $(shell find src -name '*.c' | sort)
OBJ := $(addprefix $(OBJ_DIR)/, $(SRC:.c=.o))

TEST_BIN := $(BUILD_DIR)/tests
TEST_SRC := $(shell find test -name 'test_*.c' | sort)
TEST_OBJ := $(addprefix $(OBJ_DIR)/, $(TEST_SRC:.c=.o))

FUZZ_SRC := $(shell find test -maxdepth 1 -name 'fuzz_*.c' | sort)
FUZZ_OBJ := $(addprefix $(OBJ_DIR)/, $(FUZZ_SRC:.c=.o))
FUZZ_BINS := $(patsubst test/%.c,$(BUILD_DIR)/%,$(FUZZ_SRC))

DEPS := $(OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(FUZZ_OBJ:.o=.d)

.PHONY: all
all: $(BIN) $(TEST_BIN) $(FUZZ_BINS)

.PHONY: help
help:
	@printf "Targets:\n"
	@printf "  make          - build everything\n"
	@printf "  make test     - run unit tests\n"
	@printf "  make fuzz     - run all fuzz tests\n"
	@printf "  make fuzz-xxx - run specific fuzz test\n"

$(BIN): $(OBJ)
	@$(CC) $(CFLAGS) $(OPT) -o $@ $^
	@printf "[$(CC)] Built target $@\n"

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf "[$(CC)] \033[32mBuilding C object $@\033[0m\n"
	@$(CC) $(CFLAGS) $(OPT) -c -o $@ $<

.PHONY: test
test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJ) $(OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(OPT) -o $@ $^
	@printf "[$(CC)] Built target $@\n"

.PHONY: fuzz
fuzz: check-fuzz-layout $(FUZZ_BINS)
	@for f in $(FUZZ_BINS); do $$f; done

.PHONY: fuzz-%
fuzz-%: $(BUILD_DIR)/fuzz_%
	./$< -max_total_time=10

$(BUILD_DIR)/fuzz_%: test/fuzz_%.o $(OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(OPT) -fsanitize=fuzzer,address,undefined-behavior -o $@ $^
	@printf "[$(CC)] Built target $@\n"

.PHONY: check-fuzz-layout
check-fuzz-layout:
	# Fuzz tests must live directly under 'test/' (no subdirs)
	@bad=$$(find test -mindepth 2 -name 'fuzz_*.c'); \
	if [ -n "$$bad" ]; then \
		printf "[make] \033[31mError: fuzz tests not allowed inside subdirectory: \033[0m%s\n" "$$bad"; \
		exit 1; \
	fi

.PHONY: clean
clean:
	@printf "[make] \033[33mRemoving build directory '$(BUILD_DIR)'\033[0m\n"
	@rm -rf $(BUILD_DIR)

-include $(DEPS)
