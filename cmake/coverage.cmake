# ---- Coverage target ----
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(
        COVERAGE_TRACE_COMMAND
        lcov -c -q
        -o "${CMAKE_BINARY_DIR}/coverage.info"
        -d "${CMAKE_BINARY_DIR}"
        --include "${PROJECT_SOURCE_DIR}/*"
        --ignore-errors inconsistent,source
        --filter missing
        CACHE STRING
        "; separated command to generate a trace for the 'coverage' target"
    )

    set(
        COVERAGE_HTML_COMMAND
        genhtml --legend -f -q
        "${CMAKE_BINARY_DIR}/coverage.info"
        -p "${PROJECT_SOURCE_DIR}"
        -o "${CMAKE_BINARY_DIR}/coverage_html"
        --ignore-errors inconsistent
        --branch-coverage
        --rc branch_coverage=1
        CACHE STRING
        "; separated command to generate an HTML report for the 'coverage' target"
    )

    set(
        COVERAGE_REPORT_COMMAND
        lcov --list "${CMAKE_BINARY_DIR}/coverage.info"
        --ignore-errors inconsistent,source
        --filter missing
        CACHE STRING
        "; separated command to generate report for the 'coverage' target"
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(PROF_RAW_PATH ${CMAKE_BINARY_DIR}/${LIBRARY_NAME}.profraw)
    set(PROF_DATA_PATH ${CMAKE_BINARY_DIR}/${LIBRARY_NAME}.profdata)

    set(
        COVERAGE_TRACE_COMMAND
        llvm-profdata merge
        -output=${PROF_DATA_PATH}
        ${PROF_RAW_PATH}
        CACHE STRING
        "; separated command to generate a trace for the 'coverage' target"
    )

    set(
        COVERAGE_HTML_COMMAND
        llvm-cov show $<TARGET_FILE:${LIBRARY_NAME}>
        -instr-profile=${PROF_DATA_PATH}
        -show-line-counts-or-regions
        -output-dir=${CMAKE_BINARY_DIR}/coverage_html
        -format=html
        CACHE STRING
        "; separated command to generate an HTML report for the 'coverage' target"
    )

    set(
        COVERAGE_REPORT_COMMAND
        llvm-cov report $<TARGET_FILE:${LIBRARY_NAME}>
        -instr-profile=${PROF_DATA_PATH}
        -use-color
        CACHE STRING
        "; separated command to generate report for the 'coverage' target"
    )

endif()

add_custom_target(
    coverage-report
    COMMAND ${COVERAGE_TRACE_COMMAND}
    COMMAND ${COVERAGE_REPORT_COMMAND}
    COMMENT "Generating coverage report"
    VERBATIM
)

add_custom_target(
    coverage-html
    COMMAND ${COVERAGE_TRACE_COMMAND}
    COMMAND ${COVERAGE_HTML_COMMAND}
    COMMENT "Generating coverage report"
    VERBATIM
)
