cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
project(chessplusplus VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "Debug" "RelWithDebInfo")
set(LOG_LEVEL 0 CACHE STRING "Logging level")
set(ECO_CODES_FILE "${PROJECT_SOURCE_DIR}/tools/regression/scid.eco" CACHE STRING "File with ECO codes")
set(SYZYGY_TEST_DIR "${PROJECT_SOURCE_DIR}/tests/syzygy" CACHE STRING "Directory with syzygy tablebases for unit tests")

message(STATUS "${SYZYGY_TEST_DIR}")

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(BUILD_FOR_WINDOWS True)
endif()

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -march=native -mtune=native -mpopcnt)
endif()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (MSVC)
        add_compile_options(/Od /Ob1)
    else()
        add_compile_options(-g -O0)
    endif()
    add_compile_definitions(DEBUG)
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if (MSVC)
        add_compile_options(/O2 /Ot /Ob2)
    else()
        add_compile_options(-Ofast -g)
    endif()
    add_compile_definitions(DEBUG)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (MSVC)
        add_compile_options(/O2 /Ot /Ob2)
    else()
        add_compile_options(-Ofast)
    endif()
    add_compile_definitions(NDEBUG)
else()
    message(FATAL_ERROR "Unknown build type : ${CMAKE_BUILD_TYPE}")

endif()


file(GLOB engine_src "engine/*.cpp")
list(REMOVE_ITEM engine_src "${PROJECT_SOURCE_DIR}/engine/main.cpp")
list(APPEND engine_src "${PROJECT_SOURCE_DIR}/engine/syzygy/tbprobe.cpp")

add_library(engine_objs OBJECT ${engine_src})
target_compile_options(engine_objs PRIVATE -flto -DLOG_LEVEL=${LOG_LEVEL})
target_include_directories(engine_objs
    PUBLIC "${PROJECT_SOURCE_DIR}/engine")

# engine
add_executable(chessplusplus engine/main.cpp)
target_compile_options(chessplusplus PRIVATE -flto -DLOG_LEVEL=${LOG_LEVEL})
target_link_libraries(chessplusplus PUBLIC engine_objs)
target_include_directories(chessplusplus
    PUBLIC "${PROJECT_SOURCE_DIR}/engine")

get_target_property(CHESSPLUSPLUS_CFLAGS chessplusplus COMPILE_OPTIONS)
message(STATUS "chessplusplus compile flags: ${CHESSPLUSPLUS_CFLAGS}")

# regression
if (NOT BUILD_FOR_WINDOWS)
    file(GLOB regression_src "tools/regression/*.cpp")
    add_executable(regression ${regression_src})
    target_compile_options(regression PRIVATE "-DECO_CODES_FILE=\"${ECO_CODES_FILE}\"")
    target_link_libraries(regression PUBLIC engine_objs)
    target_include_directories(regression
        PUBLIC
            "${PROJECT_SOURCE_DIR}/tools/regression"
            "${PROJECT_SOURCE_DIR}/engine")
endif()

enable_testing()

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)

# tests
file(GLOB tests_src "tests/*.cpp")
add_executable(unitTests ${tests_src})
target_compile_options(unitTests PRIVATE -fno-lto -DSYZYGY_TEST_DIR=\"${SYZYGY_TEST_DIR}\")
target_link_libraries(unitTests
    PUBLIC
        engine_objs
    PRIVATE
        gtest_main)
target_include_directories(unitTests
    PUBLIC
        "${PROJECT_BINARY_DIR}"
        "${PROJECT_SOURCE_DIR}/tests"
        "${PROJECT_SOURCE_DIR}/engine")

configure_file(tests/run_perft_tests.sh run_perft_tests.sh)
configure_file(tests/run_search_tests.sh run_search_tests.sh)

add_test(NAME unitTests COMMAND "./unitTests")
add_test(NAME perftTests COMMAND "./tests/run_perft_tests.sh" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
add_test(NAME searchTests COMMAND "./tests/run_search_tests.sh" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
