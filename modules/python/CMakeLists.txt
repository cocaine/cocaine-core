FIND_PACKAGE(PythonLibs REQUIRED)

ADD_LIBRARY(python MODULE
    python
    log)

TARGET_LINK_LIBRARIES(python
    cocaine-core
    ${PYTHON_LIBRARIES})

INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_DIRS})

SET_TARGET_PROPERTIES(python PROPERTIES
    COMPILE_FLAGS "-fno-strict-aliasing"
    PREFIX ""
    SUFFIX ".cocaine-module")

INSTALL(TARGETS python
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
