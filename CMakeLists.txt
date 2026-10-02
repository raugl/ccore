cmake_minimum_required(VERSION 3.24)
project(ccore VERSION 0.1.0 LANGUAGES C)

# TODO: Change dir names:
#  test -> tests
#  include/core -> include/ccore

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

include(GNUInstallDirs)

option(CCORE_DEBUG_ALLOCATOR "Enable allocator validation" OFF)
option(CCORE_BUILD_TESTS     "Build ccore tests" ${PROJECT_IS_TOP_LEVEL})
option(CCORE_SANITIZE        "Build ccore with ASan/UBSan in Debug" OFF)
option(CCORE_INSTALL         "Generate install rules" ${PROJECT_IS_TOP_LEVEL})
set(CCORE_FUZZ_MAX_TOTAL_TIME 10 CACHE STRING "Seconds each fuzz test runs")

if(MSVC)
    message(FATAL_ERROR "ccore requires GNU C extensions (GCC or Clang)")
endif()

if(PROJECT_IS_TOP_LEVEL)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

add_library(ccore_warnings INTERFACE)
target_compile_options(ccore_warnings INTERFACE
    -Wall -Wextra -Wpedantic
    -Wshadow -Wundef -Wcast-qual
    -Wformat=2 -Wstrict-overflow=5
    -Wuninitialized
    -Wconversion -Wsign-conversion -Wdouble-promotion
    -Wswitch-enum -Wswitch-default -Wimplicit-fallthrough
    -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes
    -Wwrite-strings -Wnull-dereference
    -Wno-unused-function
)

add_library(ccore_options INTERFACE)
if(CCORE_SANITIZE)
    target_compile_options(ccore_options INTERFACE "$<$<CONFIG:Debug,RelWithDebInfo>:-fsanitize=address,undefined>")
    target_link_options   (ccore_options INTERFACE "$<$<CONFIG:Debug,RelWithDebInfo>:-fsanitize=address,undefined>")
endif()

add_library(ccore
    src/common.c
    src/hash.c
    src/testing.c
    src/tlsf_allocator.c
    src/arena_allocator.c
)
add_library(ccore::ccore ALIAS ccore)
target_compile_features(ccore PUBLIC c_std_11) # consumers need at least C11

target_compile_definitions(ccore PUBLIC "$<$<CONFIG:Debug,RelWithDebInfo>:CCORE_DEBUG>")
target_include_directories(ccore PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(ccore PRIVATE
    "$<BUILD_INTERFACE:ccore_warnings>"
    "$<BUILD_INTERFACE:ccore_options>"
)
if(CCORE_DEBUG_ALLOCATOR)
    target_compile_definitions(ccore PRIVATE CCORE_DEBUG_ALLOCATOR)
endif()

if(CCORE_BUILD_TESTS)
    enable_testing()

    add_executable(ccore_tests test/test_main.c)
    target_link_libraries(ccore_tests PRIVATE ccore ccore_warnings ccore_options)
    add_test(NAME ccore_tests COMMAND ccore_tests)
endif()

# NOTE: Fuzz tests just use libfuzzer, so they depend on Clang
if(CCORE_BUILD_TESTS AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_custom_target(ccore_fuzz_tests)

    set(CCORE_FUZZ_SOURCES
        test/fuzz_deque.c
        test/fuzz_pqueue.c
        test/fuzz_hashmap.c
    )
    foreach(source IN LISTS CCORE_FUZZ_SOURCES)
        get_filename_component(name "${source}" NAME_WE)
        string(REGEX REPLACE "^fuzz_" "" name "${name}")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.fuzz-corpus/${name}")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/test/corpus/${name}")

        set(target "ccore_fuzz_${name}")
        add_executable(${target} "${source}")
        target_compile_options(${target} PRIVATE -fsanitize=fuzzer)
        target_link_options   (${target} PRIVATE -fsanitize=fuzzer)
        target_link_libraries (${target} PRIVATE ccore ccore_warnings ccore_options)

        add_test(NAME ${target} COMMAND ${target}
            "-max_total_time=${CCORE_FUZZ_MAX_TOTAL_TIME}"
            "${CMAKE_CURRENT_SOURCE_DIR}/.fuzz-corpus/${name}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/corpus/${name}"
        )
        set_tests_properties(${target} PROPERTIES LABELS fuzz)
        add_dependencies(ccore_fuzz_tests ${target})
    endforeach()
endif()

if(CCORE_INSTALL)
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(ccoreConfigVersion.cmake COMPATIBILITY SameMajorVersion)

    install(TARGETS ccore EXPORT ccoreTargets)
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT ccoreTargets
        NAMESPACE ccore::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ccore"
    )
    install(FILES
        cmake/ccoreConfig.cmake
        "${CMAKE_CURRENT_BINARY_DIR}/ccoreConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ccore"
    )
endif()
