ADD_LIBRARY(yappi-client SHARED
    client.cpp)

TARGET_LINK_LIBRARIES(yappi-client
    json
    ${UUID_LIBS}
    boost_thread-mt
    zmq)

SET_TARGET_PROPERTIES(yappi-client PROPERTIES
    VERSION 1
    PUBLIC_HEADER "client.hpp")

INSTALL(TARGETS yappi-client
    LIBRARY DESTINATION lib COMPONENT runtime
    PUBLIC_HEADER DESTINATION include/yappi COMPONENT development)
