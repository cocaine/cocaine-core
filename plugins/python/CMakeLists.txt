EXECUTE_PROCESS(
    COMMAND python-config --includes
    OUTPUT_VARIABLE PYTHON_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

EXECUTE_PROCESS(
    COMMAND python-config --libs
    OUTPUT_VARIABLE PYTHON_LDFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

IF(NOT PYTHON_CFLAGS OR NOT PYTHON_LDFLAGS)
    MESSAGE(WARNING "Python development files are required to build this plugin.")
    RETURN()
ENDIF()

FIND_PACKAGE(CURL)

IF(NOT CURL_FOUND)
    MESSAGE(WARNING "CURL development files are required to build this plugin.")
    RETURN()
ENDIF()

ADD_LIBRARY(yappi-plugin-python MODULE
    python)

TARGET_LINK_LIBRARIES(yappi-plugin-python
    crypto
    curl
    uriparser)

SET_TARGET_PROPERTIES(yappi-plugin-python PROPERTIES
    COMPILE_FLAGS "${PYTHON_CFLAGS}"
    LINK_FLAGS "${PYTHON_LDFLAGS}")

INSTALL(TARGETS yappi-plugin-python
    LIBRARY DESTINATION lib/yappi
    COMPONENT runtime)
