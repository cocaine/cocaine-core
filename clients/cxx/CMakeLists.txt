ADD_LIBRARY(cocaine-client SHARED
    client.cpp)

TARGET_LINK_LIBRARIES(cocaine-client
    json
    ${UUID_LIBS}
    boost_thread-mt
    zmq)

SET_TARGET_PROPERTIES(cocaine-client PROPERTIES
    VERSION 1
    PUBLIC_HEADER "client.hpp")

INSTALL(TARGETS cocaine-client
    LIBRARY DESTINATION lib COMPONENT runtime
    PUBLIC_HEADER DESTINATION include/cocaine COMPONENT development)
