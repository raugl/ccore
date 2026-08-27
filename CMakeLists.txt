cmake_minimum_required(VERSION 3.24)
project(ccore LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CCORE_FUZZ_MAX_TOTAL_TIME 10 CACHE STRING "Seconds each fuzz test runs")

add_library(ccore_warnings INTERFACE)
target_compile_options(ccore_warnings INTERFACE
    -Wall -Wextra -Wpedantic
    -Wshadow -Wundef -Wcast-qual
    -Wformat=2 -Wstrict-overflow=5
    -Wconversion -Wsign-conversion -Wdouble-promotion
    -Wswitch-enum -Wimplicit-fallthrough
    -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes
    -Wwrite-strings -Wnull-dereference
    -Wno-unused-function
)

add_library(ccore_options INTERFACE)
target_compile_options(ccore_options INTERFACE "$<$<CONFIG:Debug,RelWithDebInfo>:-fsanitize=address,undefined>")
target_link_options(ccore_options INTERFACE "$<$<CONFIG:Debug,RelWithDebInfo>:-fsanitize=address,undefined>")

add_library(ccore STATIC
    src/allocator.c
    src/common.c
    src/hash.c
    src/testing.c
)
add_library(ccore::ccore ALIAS ccore)
target_include_directories(ccore PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(ccore PRIVATE ccore_warnings ccore_options)

enable_testing()

add_executable(tests
    test/test_main.c
)
target_compile_definitions(tests PRIVATE TESTING)
target_link_libraries(tests PRIVATE ccore ccore_warnings ccore_options)
add_test(NAME tests COMMAND tests)

set(CCORE_FUZZ_SOURCES
    test/fuzz_deque.c
    test/fuzz_pqueue.c
)
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    foreach(source IN LISTS CCORE_FUZZ_SOURCES)
        get_filename_component(name "${source}" NAME_WE)

        add_executable("${name}" "${source}")
        target_compile_definitions(${name} PRIVATE FUZZING)
        target_compile_options("${name}" PRIVATE -fsanitize=fuzzer)
        target_link_options("${name}" PRIVATE -fsanitize=fuzzer)
        target_link_libraries("${name}" PRIVATE ccore ccore_warnings ccore_options)

        add_test(NAME "${name}" COMMAND "${name} -max_total_time=${CCORE_FUZZ_MAX_TOTAL_TIME}")
        set_tests_properties("${name}" PROPERTIES LABELS fuzz)
    endforeach()
endif()
