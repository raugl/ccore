# CMAKE_BUILD_TYPE can be overridden per invocation, e.g.:
#   make test CMAKE_BUILD_TYPE=RelWithDebInfo
#
# Extra args after `--` are forwarded to the test/fuzz_xxx binaries, e.g.:
#   make test -- --filter=deque --seed=1234
#   make fuzz-darray -- -max_total_time=600 .fuzz-corpus/darray/ test/corpus/darray/

all:       CMAKE_BUILD_TYPE := Release
test:      CMAKE_BUILD_TYPE := Debug
fuzz:      CMAKE_BUILD_TYPE := RelWithDebInfo
fuzz-%:    CMAKE_BUILD_TYPE := RelWithDebInfo
fuzz-list: CMAKE_BUILD_TYPE := RelWithDebInfo

ifneq ($(CMAKE_C_COMPILER),)
CMAKE_ARGS += -DCMAKE_C_COMPILER=$(CMAKE_C_COMPILER)
endif

all:
	@cmake --preset $(CMAKE_BUILD_TYPE)
	@cmake --build build/$(CMAKE_BUILD_TYPE) --target ccore --parallel

help:
	@printf "Targets:\n"
	@printf "  make [all]     - build the optimized library\n"
	@printf "  make test      - run all unit tests\n"
	@printf "  make fuzz      - run all fuzz tests\n"
	@printf "  make fuzz-xxx  - run specific fuzz test\n"
	@printf "  make fuzz-list - list available fuzz tests\n"

test:
	cmake --preset $(CMAKE_BUILD_TYPE) $(CMAKE_ARGS)
	@cmake --build build/$(CMAKE_BUILD_TYPE) --target tests --parallel
	@build/$(CMAKE_BUILD_TYPE)/tests $(ARGS)

fuzz:
	@cmake --preset $(CMAKE_BUILD_TYPE) $(CMAKE_ARGS)
	@cmake --build build/$(CMAKE_BUILD_TYPE) --target fuzz_tests --parallel
	@cd build/$(CMAKE_BUILD_TYPE) && ctest --output-on-failure -L fuzz

fuzz-%:
	@cmake --preset $(CMAKE_BUILD_TYPE) $(CMAKE_ARGS)
	@cmake --build build/$(CMAKE_BUILD_TYPE) --target fuzz_$* --parallel
	@build/$(CMAKE_BUILD_TYPE)/fuzz_$* $(ARGS)

fuzz-list:
	@cmake --preset $(CMAKE_BUILD_TYPE) $(CMAKE_ARGS)
	@cd build/$(CMAKE_BUILD_TYPE) && ctest -N -L fuzz 2>/dev/null | sed -n 's/^ *Test #[0-9]*: fuzz_/fuzz-/p'

clean:
	@rm -rf build

.PHONY: all test fuzz fuzz-list clean
