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

ADD_LIBRARY(yappi-plugin-python MODULE
    python)

TARGET_LINK_LIBRARIES(yappi-plugin-python
    yappi-core-storage
    yappi-core-security
    ${BOOST_SYSTEM_LIBS}
    boost_thread-mt
    curl
    uriparser)

SET_TARGET_PROPERTIES(yappi-plugin-python PROPERTIES
    COMPILE_FLAGS "${PYTHON_CFLAGS} -fno-strict-aliasing"
    LINK_FLAGS "${PYTHON_LDFLAGS}")

INSTALL(TARGETS yappi-plugin-python
    LIBRARY DESTINATION lib/yappi
    COMPONENT runtime)
