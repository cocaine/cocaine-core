CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(Yappi)

FIND_PACKAGE(Boost 1.34.1 REQUIRED COMPONENTS
    thread 
    filesystem 
    program_options)

FIND_PACKAGE(OpenSSL REQUIRED)
FIND_PACKAGE(CURL REQUIRED)

INCLUDE(LocateLibrary.cmake)

LOCATE_LIBRARY(LIBZMQ "zmq.hpp" "zmq")
LOCATE_LIBRARY(LIBEV "ev++.h" "ev")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBURIPARSER "uriparser/Uri.h" "uriparser")

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${LIBZMQ_INCLUDE_DIRS}
    ${LIBEV_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBURIPARSER_INCLUDE_DIRS})

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBZMQ_LIBRARY_DIRS}
    ${LIBEV_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS}
    ${LIBURIPARSER_LIBRARY_DIRS})

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/foreign/jsoncpp-0.5.0/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

IF(NOT APPLE)
    SET(LIBUUID uuid)
ENDIF()

ADD_LIBRARY(json
    foreign/jsoncpp-0.5.0/src/lib_json/json_value
    foreign/jsoncpp-0.5.0/src/lib_json/json_reader
    foreign/jsoncpp-0.5.0/src/lib_json/json_writer)

SET_TARGET_PROPERTIES(json PROPERTIES
    COMPILE_FLAGS -fPIC)

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
    ${LIBUUID}
    boost_system-mt
    boost_thread-mt
    boost_filesystem-mt
    zmq
    ev
    msgpack
    uriparser)

SET(YAPPI_HEADERS
    include/plugin.hpp
    include/uri.hpp
    include/digest.hpp)

SET_TARGET_PROPERTIES(yappi-core PROPERTIES
    VERSION 1
    COMPILE_DEFINITIONS "BOOST_FILESYSTEM_VERSION=2"
    PUBLIC_HEADER "${YAPPI_HEADERS}")

ADD_DEPENDENCIES(yappi-core json)

ADD_EXECUTABLE(yappi src/main)

TARGET_LINK_LIBRARIES(yappi
    yappi-core
    boost_program_options-mt)

ADD_DEPENDENCIES(yappi yappi-core)

ADD_SUBDIRECTORY(plugins)
ADD_SUBDIRECTORY(clients)
ADD_SUBDIRECTORY(toolset)

INSTALL(TARGETS yappi yappi-core
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime NAMELINK_SKIP
    PUBLIC_HEADER DESTINATION include/yappi COMPONENT development)
