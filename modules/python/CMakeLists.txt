IF (PYTHON)
    EXECUTE_PROCESS(
        COMMAND python-config --includes
        OUTPUT_VARIABLE PYTHON_CFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    EXECUTE_PROCESS(
        COMMAND python-config --libs
        OUTPUT_VARIABLE PYTHON_LDFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    IF(NOT PYTHON_CFLAGS OR NOT PYTHON_LDFLAGS)
        MESSAGE(FATAL_ERROR "Python development files are required to build.")
        RETURN()
    ENDIF()

    ADD_LIBRARY(python MODULE
        python
        log
        io)

    TARGET_LINK_LIBRARIES(python
        cocaine-core)

    SET_TARGET_PROPERTIES(python PROPERTIES
        COMPILE_FLAGS "${PYTHON_CFLAGS} -fno-strict-aliasing"
        LINK_FLAGS "${PYTHON_LDFLAGS}"
        PREFIX ""
        SUFFIX ".cocaine-module")

    INSTALL(TARGETS python
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF()
