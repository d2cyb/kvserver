cmake_minimum_required(VERSION 3.31)

project(
    ${PROJECT_NAME}_unit
)
set(EXECUTABLE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

message(STATUS "Unit tests enabled")

set(UNIT_TESTS_PROJECT_NAME ${PROJECT_NAME})


find_package(Catch2 3 REQUIRED)
find_package(Boost CONFIG)
find_package(${LIBRARY_NAME} PATHS ${CMAKE_BINARY_DIR})

file(
    GLOB_RECURSE
    UNIT_TEST_FILES
    ${PROJECT_SOURCE_DIR}/src/config.test.cpp
    ${PROJECT_SOURCE_DIR}/src/server.test.cpp
    ${PROJECT_SOURCE_DIR}/src/statistic.test.cpp
    ${PROJECT_SOURCE_DIR}/src/utilities.test.cpp
)

add_executable(${UNIT_TESTS_PROJECT_NAME})
target_sources(${UNIT_TESTS_PROJECT_NAME}
    PRIVATE
        ${UNIT_TEST_FILES}
)

set_target_properties(
    ${UNIT_TESTS_PROJECT_NAME}
    PROPERTIES
    CMAKE_CXX_EXTENSIONS OFF
    CMAKE_CXX_VISIBILITY_PRESET hidden
    CMAKE_VISIBILITY_INLINES_HIDDEN ON
    CXX_STANDARD 20
)

target_include_directories(${UNIT_TESTS_PROJECT_NAME}
    PRIVATE "${CMAKE_BINARY_DIR}"
)

target_link_libraries(${UNIT_TESTS_PROJECT_NAME} PRIVATE
    ${LIBRARY_NAME}
    Catch2::Catch2WithMain
    boost_thread
    pthread
)

include(Catch)
catch_discover_tests(${UNIT_TESTS_PROJECT_NAME})

# Run test after build
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Setting the environment for generating a profraw file for coverage calculation with llvm
    set(RUN_UNIT_TEST_COMMAND LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/${LIBRARY_NAME}.profraw $<TARGET_FILE:${UNIT_TESTS_PROJECT_NAME}>)
else()
    set(RUN_UNIT_TEST_COMMAND ${UNIT_TESTS_PROJECT_NAME})
endif()

set(ENABLE_AUTO_RUN_TESTS OFF)
if (ENABLE_AUTO_RUN_TESTS)
    add_custom_command(
        TARGET ${UNIT_TESTS_PROJECT_NAME}
        COMMENT "Run tests"
        POST_BUILD
        COMMAND ${RUN_UNIT_TEST_COMMAND}
    )
endif()
