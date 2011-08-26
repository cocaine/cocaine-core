ADD_LIBRARY(yappi-plugin-inject MODULE
    inject.cpp)

TARGET_LINK_LIBRARIES(yappi-plugin-inject
    zmq)
