ADD_LIBRARY(yappi-plugin-inject MODULE
    inject.cpp)

TARGET_LINK_LIBRARIES(yappi-plugin-inject
    yappi-core-security
    zmq
    uriparser)

ADD_DEPENDENCIES(yappi-plugin-inject
    yappi-core-security)

INSTALL(TARGETS yappi-plugin-inject
    LIBRARY DESTINATION lib/yappi
    COMPONENT runtime)
