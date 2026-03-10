cmake_minimum_required(VERSION 3.31)

project(
    ${PROJECT_NAME}_bench
)
set(EXECUTABLE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

message(STATUS "Performing tests enabled")

set(BENCHMARK_TESTS_PROJECT_NAME ${PROJECT_NAME})

find_package(Catch2 3 REQUIRED)
find_package(Boost CONFIG)
find_package(${LIBRARY_NAME} PATHS ${CMAKE_BINARY_DIR})

file(
    GLOB_RECURSE
    UNIT_TEST_FILES
    ${PROJECT_SOURCE_DIR}/src/server.bench.cpp
    ${PROJECT_SOURCE_DIR}/src/statistic.bench.cpp
)

add_executable(${BENCHMARK_TESTS_PROJECT_NAME})
target_sources(${BENCHMARK_TESTS_PROJECT_NAME}
    PRIVATE
        ${UNIT_TEST_FILES}
)

set_target_properties(
    ${BENCHMARK_TESTS_PROJECT_NAME}
    PROPERTIES
    CMAKE_CXX_EXTENSIONS OFF
    CMAKE_CXX_VISIBILITY_PRESET hidden
    CMAKE_VISIBILITY_INLINES_HIDDEN ON
    CXX_STANDARD 20
)

target_include_directories(${BENCHMARK_TESTS_PROJECT_NAME}
    PRIVATE "${CMAKE_BINARY_DIR}"
)

target_link_libraries(${BENCHMARK_TESTS_PROJECT_NAME} PRIVATE
    ${LIBRARY_NAME}
    Catch2::Catch2WithMain
    boost_thread
    pthread

)

include(Catch)
catch_discover_tests(${BENCHMARK_TESTS_PROJECT_NAME})
