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

INCLUDE_DIRECTORIES(
    include)

ADD_LIBRARY(cocaine-plugin-python MODULE
    src/common
    src/python
    src/storage)

TARGET_LINK_LIBRARIES(cocaine-plugin-python
    cocaine-core-config
    cocaine-core-storage
    cocaine-core
    json
    ${BOOST_SYSTEM_LIBS}
    boost_filesystem-mt
    uriparser)

SET_TARGET_PROPERTIES(cocaine-plugin-python PROPERTIES
    COMPILE_FLAGS "${PYTHON_CFLAGS} -fno-strict-aliasing"
    LINK_FLAGS "${PYTHON_LDFLAGS}")

INSTALL(TARGETS cocaine-plugin-python
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
