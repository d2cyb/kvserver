cmake_minimum_required(VERSION 3.31)

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

find_package(Boost CONFIG REQUIRED COMPONENTS json thread container atomic chrono date_time)
find_package(${LIBRARY_NAME} PATHS ${CMAKE_BINARY_DIR})

file(GLOB_RECURSE KVSERVER_SOURCE_FILES
    ${PROJECT_SOURCE_DIR}/src/server.cpp
    ${PROJECT_SOURCE_DIR}/src/session.cpp
    ${PROJECT_SOURCE_DIR}/src/config.cpp
    ${PROJECT_SOURCE_DIR}/src/statistic.cpp
    ${PROJECT_SOURCE_DIR}/src/utilities.cpp
)

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

target_include_directories(${LIBRARY_NAME} PRIVATE
    "${CMAKE_BINARY_DIR}"
)

target_link_libraries(${LIBRARY_NAME} PUBLIC
    boost_json
    boost_thread
    boost_container
    boost_atomic
    boost_chrono
    boost_date_time
    pthread
)

target_include_directories(${LIBRARY_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(${APP_EXECUTABLE_NAME}
    ${PROJECT_SOURCE_DIR}/src/main.cpp
)
set_target_properties(
    ${APP_EXECUTABLE_NAME}
    PROPERTIES
    CMAKE_CXX_EXTENSIONS OFF
    CMAKE_CXX_VISIBILITY_PRESET hidden
    CMAKE_VISIBILITY_INLINES_HIDDEN ON
    CXX_STANDARD 20
)
target_compile_features(${APP_EXECUTABLE_NAME} PUBLIC cxx_std_20)
target_link_libraries(${APP_EXECUTABLE_NAME} PRIVATE
    ${LIBRARY_NAME}
)
target_include_directories(${APP_EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
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

if(NOT kvserver_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of kvserver"
  )
endif()

include(cmake/dev-mode.cmake)
