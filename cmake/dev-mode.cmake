include(cmake/folders.cmake)

include(CTest)
option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

option(BUILD_MCSS_DOCS "Build documentation using Doxygen and m.css" OFF)
if(BUILD_MCSS_DOCS)
  include(cmake/docs.cmake)
endif()

option(ENABLE_COVERAGE "Enable coverage support separate from CTest's" ON)
if(ENABLE_COVERAGE)
  include(cmake/coverage.cmake)
endif()

include(cmake/lint-targets.cmake)
include(cmake/spell-targets.cmake)
include(cmake/sanitize-targets.cmake)

add_folders(Project)
