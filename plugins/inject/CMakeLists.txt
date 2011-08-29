ADD_LIBRARY(yappi-plugin-inject MODULE
    inject.cpp)

TARGET_LINK_LIBRARIES(yappi-plugin-inject
    crypto
    zmq
    uriparser)

INSTALL(TARGETS yappi-plugin-inject
    LIBRARY DESTINATION lib/${PROJECT_NAME}
    COMPONENT runtime)
