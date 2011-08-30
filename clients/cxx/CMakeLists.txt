ADD_LIBRARY(yappi-client-cxx SHARED
    client.cpp)

TARGET_LINK_LIBRARIES(yappi-client-cxx
    json
    ${LIBUUID_LINK_FLAGS}
    boost_thread-mt
    zmq)

SET(YAPPI_CLIENT_CXX_HEADERS
    client.hpp)

SET_TARGET_PROPERTIES(yappi-client-cxx PROPERTIES
    VERSION 1
    PUBLIC_HEADER "${YAPPI_CLIENT_CXX_HEADERS}")

INSTALL(TARGETS yappi-client-cxx
    LIBRARY DESTINATION lib COMPONENT runtime
    PUBLIC_HEADER DESTINATION include/yappi COMPONENT development)
