FIND_PROGRAM(GCOV_PATH gcov)
FIND_PROGRAM(LCOV_PATH lcov)
FIND_PROGRAM(GENHTML_PATH genhtml)

IF(NOT GCOV_PATH)
    MESSAGE(FATAL_ERROR "gcov not found! Aborting...")
ENDIF() # NOT GCOV_PATH

IF(NOT LCOV_PATH)
    MESSAGE(FATAL_ERROR "lcov not found! Aborting...")
ENDIF() # NOT LCOV_PATH

IF(NOT GENHTML_PATH)
    MESSAGE(FATAL_ERROR "genhtml not found! Aborting...")
ENDIF() # NOT GENHTML_PATH

SET(COVERAGE_CXX_FLAGS
    "--coverage -fprofile-arcs -ftest-coverage"
    CACHE STRING "Flags used by the C++ compiler during coverage builds."
    FORCE
)

SET(COVERAGE_LIBRARIES -fprofile-arcs -ftest-coverage)

MARK_AS_ADVANCED(
    COVERAGE_CXX_FLAGS
    COVERAGE_LIBRARIES
)

FUNCTION(SETUP_TARGET_FOR_COVERAGE _targetname _testrunner _outputname)
    ADD_CUSTOM_TARGET(${_targetname}
        # Reset all execution counts to zero.
        ${LCOV_PATH} --directory . --zerocounters

        # Run tests.
        COMMAND ${_testrunner} ${ARGV3}

        # Capture coverage data.
        COMMAND ${LCOV_PATH} --compat-libtool --directory . --capture --output-file ${_outputname}.info
        COMMAND ${LCOV_PATH} --remove ${_outputname}.info 'tests/*' '/usr/*' 'foreign/*' --output-file ${_outputname}.info.cleaned

        # Generating the report.
        COMMAND ${GENHTML_PATH} -o ${_outputname} ${_outputname}.info.cleaned
        # COMMAND ${CMAKE_COMMAND} -E remove ${_outputname}.info ${_outputname}.info.cleaned
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Resetting code coverage counters to zero.\nProcessing code coverage counters and generating report."
    )

    # Show info where to find the report
    ADD_CUSTOM_COMMAND(TARGET ${_targetname} POST_BUILD
        COMMAND ;
        COMMENT "Open ./${_outputname}/index.html in your browser to view the coverage report."
    )
ENDFUNCTION() # SETUP_TARGET_FOR_COVERAGE
