cmake_minimum_required(VERSION 3.14)

macro(default name)
  if(NOT DEFINED "${name}")
    set("${name}" "${ARGN}")
  endif()
endmacro()

default(FORMAT_COMMAND clang-format)
default(
    PATTERNS
    src/*.cpp
    tests/*.cpp
)
default(FIX NO)

set(flag --output-replacements-xml)
set(args OUTPUT_VARIABLE output)
if(FIX)
  set(flag -i)
  set(args "")
endif()

file(GLOB_RECURSE files ${PATTERNS})
set(badly_formatted "")
set(output "")
string(LENGTH "${CMAKE_SOURCE_DIR}/" path_prefix_length)

foreach(file IN LISTS files)
  execute_process(
      COMMAND "${FORMAT_COMMAND}" --style=file "${flag}" "${file}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      RESULT_VARIABLE result
      ${args}
  )
  if(NOT result EQUAL "0")
    message(FATAL_ERROR "'${file}': formatter returned with ${result}")
  endif()
  if(NOT FIX AND output MATCHES "\n<replacement offset")
    string(SUBSTRING "${file}" "${path_prefix_length}" -1 relative_file)
    list(APPEND badly_formatted "${relative_file}")
  endif()
  set(output "")
endforeach()

if(NOT badly_formatted STREQUAL "")
  list(JOIN badly_formatted "\n" bad_list)
  message("The following files are badly formatted:\n\n${bad_list}\n")
  message(FATAL_ERROR "Run again with FIX=YES to fix these files.")
endif()

#LINTS
set(ENABLE_TIDY_CHECK OFF) # TODO (eliminate module import errors in tests and set ON)
find_program(CLANG_TIDY_COMMAND NAMES clang-tidy)
if (ENABLE_TIDY_CHECK)
    if(NOT CLANG_TIDY_COMMAND)
        message(WARNING "Could not find clang-tidy!")
        set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
    else()
        message(STATUS "clang-tidy enabled")
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
        set(
            CMAKE_CXX_CLANG_TIDY
            "clang-tidy;-format-style='file'"
            "-p=${CMAKE_BINARY_DIR}"
            --enable-module-headers-parsing
            --use-color
        )
    endif()
else()
    message(STATUS "clang-tidy disabled ")
endif()
