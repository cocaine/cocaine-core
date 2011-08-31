ADD_LIBRARY(yappi-plugin-inject MODULE
    inject.cpp)

TARGET_LINK_LIBRARIES(yappi-plugin-inject
    yappi-core-security
    zmq
    uriparser)

INSTALL(TARGETS yappi-plugin-inject
    LIBRARY DESTINATION lib/yappi
    COMPONENT runtime)
