CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(YAPPI)

FIND_FILE(LIBEV_HEADER "ev++.h" PATH_SUFFIXES "ev" "libev")
GET_FILENAME_COMPONENT(LIBEV_HEADER_PATH ${LIBEV_HEADER} PATH)

IF(NOT LIBEV_HEADER)
    MESSAGE(SEND_ERROR "libev development headers are required.")
ENDIF()

FIND_FILE(LIBZMQ_HEADER "zmq.hpp" PATH_SUFFIXES "zmq" "libzmq")
GET_FILENAME_COMPONENT(LIBZMQ_HEADER_PATH ${LIBZMQ_HEADER} PATH)

IF(NOT LIBZMQ_HEADER)
    MESSAGE(SEND_ERROR "libzmq development headers are required.")
ENDIF()

FIND_FILE(LIBMSGPACK_HEADER "msgpack.hpp" PATH_SUFFIXES "msgpack" "libmsgpack")
GET_FILENAME_COMPONENT(LIBMSGPACK_HEADER_PATH ${LIBMSGPACK_HEADER} PATH)

IF(NOT LIBMSGPACK_HEADER)
    MESSAGE(SEND_ERROR "libmsgpack development headers are required.")
ENDIF()

INCLUDE_DIRECTORIES(${LIBEV_HEADER_PATH} ${LIBZMQ_HEADER_PATH} ${LIBMSGPACK_HEADER_PATH})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/foreign/jsoncpp-0.5.0/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

ADD_LIBRARY(json
    foreign/jsoncpp-0.5.0/src/lib_json/json_value
    foreign/jsoncpp-0.5.0/src/lib_json/json_reader
    foreign/jsoncpp-0.5.0/src/lib_json/json_writer)

ADD_LIBRARY(yappi-core SHARED 
    src/core
    src/engine
    src/registry
    src/sockets
    src/file_storage
    src/security)

TARGET_LINK_LIBRARIES(yappi-core
    json
    crypto
    dl
    boost_thread
    boost_filesystem
    ev
    zmq
    msgpack
    uriparser)

ADD_DEPENDENCIES(yappi-core json)

ADD_EXECUTABLE(yappi src/main)

TARGET_LINK_LIBRARIES(yappi
    yappi-core
    boost_program_options)

ADD_DEPENDENCIES(yappi yappi-core)

SET_TARGET_PROPERTIES(yappi-core PROPERTIES
    VERSION 1)

ADD_SUBDIRECTORY(plugins)
