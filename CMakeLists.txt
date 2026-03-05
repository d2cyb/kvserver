cmake_minimum_required(VERSION 3.28)

project(kvserver
        VERSION 0.0.1
        LANGUAGES CXX)

set(APP_EXECUTABLE_NAME ${PROJECT_NAME})

set(LIBRARY_NAME ${PROJECT_NAME}_lib)
set(LIBRARY_OUTPUT_NAME "kvserver_lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost COMPONENTS thread json)

file(GLOB_RECURSE KVSERVER_SOURCE_FILES
    ${PROJECT_SOURCE_DIR}/src/server.cpp
    ${PROJECT_SOURCE_DIR}/src/session.cpp
    ${PROJECT_SOURCE_DIR}/src/config.cpp
    ${PROJECT_SOURCE_DIR}/src/statistic.cpp
    ${PROJECT_SOURCE_DIR}/src/thread_pool.cpp
)

add_executable(${APP_EXECUTABLE_NAME}
    ${PROJECT_SOURCE_DIR}/src/main.cpp
)
set_target_properties(
    ${APP_EXECUTABLE_NAME}
    PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    THREADS_PREFER_PTHREAD_FLAG ON
)
target_compile_features(${APP_EXECUTABLE_NAME} PUBLIC cxx_std_20)

add_library(${LIBRARY_NAME})
target_sources(${LIBRARY_NAME}
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ${KVSERVER_SOURCE_FILES}
)
set_target_properties(
    ${LIBRARY_NAME}
    PROPERTIES
    CMAKE_CXX_EXTENSIONS OFF
    CMAKE_CXX_VISIBILITY_PRESET hidden
    CMAKE_VISIBILITY_INLINES_HIDDEN ON
    CXX_STANDARD 20
)

include(GenerateExportHeader)
generate_export_header(${LIBRARY_NAME})
target_sources(${LIBRARY_NAME}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/kvserver_lib_export.h
)

target_include_directories(${LIBRARY_NAME}
    PRIVATE "${CMAKE_BINARY_DIR}"
)

target_link_libraries(${LIBRARY_NAME} PRIVATE
    Boost::thread
    Boost::json
    pthread
)

target_include_directories(${LIBRARY_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(${LIBRARY_NAME} PROPERTIES VERSION ${PROJECT_VERSION})

target_compile_features(${LIBRARY_NAME} PUBLIC cxx_std_20)

include(GNUInstallDirs)
install(TARGETS ${LIBRARY_NAME}
    EXPORT ${LIBRARY_NAME}Config
    FILE_SET CXX_MODULES
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${LIBRARY_NAME}/src
    FILE_SET HEADERS
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    INCLUDES
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
export(TARGETS ${LIBRARY_NAME}
    NAMESPACE ${LIBRARY_NAME}::
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_NAME}Config.cmake"
)
install(EXPORT
    ${LIBRARY_NAME}Config
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${LIBRARY_NAME}
    CXX_MODULES_DIRECTORY .
)
install(FILES ${LIBRARY_NAME}-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${LIBRARY_NAME}
)

target_compile_options(${APP_EXECUTABLE_NAME} PRIVATE
    -Wall -Wextra -Werror -pedantic
)
target_compile_options(${LIBRARY_NAME} PRIVATE
    -Wall -Wextra -Werror -pedantic
)

# lints
set(ENABLE_TIDY_CHECK ON)
find_program(CLANG_TIDY_COMMAND NAMES clang-tidy)
if (ENABLE_TIDY_CHECK)
    if(NOT CLANG_TIDY_COMMAND)
        message(WARNING "Could not find clang-tidy!")
        set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
    else()
        message(STATUS "clang-tidy enabled")
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
        set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-format-style='file'")
    endif()
else()
    message(STATUS "clang-tidy disabled ")
endif()

# ctest -T memcheck
find_program(CTEST_MEMORYCHECK_COMMAND NAMES valgrind)

# tests
add_subdirectory(tests)
