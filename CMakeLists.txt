cmake_minimum_required(VERSION 3.20)
project(myproj C)

set(CMAKE_C_STANDARD 11)

set(COMMON_FLAGS
    -Wall -Wextra -Wpedantic
    -Wshadow -Wundef -Wcast-qual
    -Wformat=2 -Wstrict-overflow=5
    -Wconversion -Wsign-conversion -Wdouble-promotion
    -Wswitch-enum -Wswitch-default -Wimplicit-fallthrough
    -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition
    -Wwrite-strings -Wnull-dereference
    -Wno-unused-function
)

add_compile_options(${COMMON_FLAGS})

# Debug-style defaults (like -g -O1)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O1")

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS src/*.c)
file(GLOB TEST_SRC CONFIGURE_DEPENDS test/test_*.c)
file(GLOB FUZZ_SRC CONFIGURE_DEPENDS test/fuzz_*.c)

add_executable(main ${SRC})
add_executable(tests ${TEST_SRC} ${SRC})

add_custom_target(run-tests
    COMMAND tests
    DEPENDS tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(check-fuzz-layout
    COMMAND ${CMAKE_COMMAND} -E echo "Checking fuzz layout..."
    COMMAND bash -c "bad=$$(find test -mindepth 2 -name 'fuzz_*.c'); \
        if [ -n \"$$bad\" ]; then \
            printf \"Error: fuzz tests not allowed in subdirectories:\\n%s\\n\" \"$$bad\"; \
            exit 1; \
        fi"
)

foreach(fuzz_file ${FUZZ_SRC})
    get_filename_component(name ${fuzz_file} NAME_WE)

    add_executable(${name} ${fuzz_file} ${SRC})

    target_compile_options(${name} PRIVATE
        -fsanitize=fuzzer,address,undefined
    )

    target_link_options(${name} PRIVATE
        -fsanitize=fuzzer,address,undefined
    )

    add_dependencies(${name} check-fuzz-layout)

    add_custom_target(run-${name}
        COMMAND ${name} -max_total_time=10
        DEPENDS ${name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endforeach()

add_custom_target(fuzz-all)

foreach(fuzz_file ${FUZZ_SRC})
    get_filename_component(name ${fuzz_file} NAME_WE)
    add_dependencies(fuzz-all ${name})

    add_custom_command(TARGET fuzz-all POST_BUILD
        COMMAND ${name}
    )
endforeach()
