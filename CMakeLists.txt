CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(yappi)

# libev
FIND_PATH(LIBEV_HEADER_PATH "ev++.h" PATH_SUFFIXES "libev")

IF(NOT LIBEV_HEADER_PATH)
    MESSAGE(FATAL_ERROR "libev development headers are required.")
ENDIF()

# libzmq
FIND_PATH(LIBZMQ_HEADER_PATH "zmq.hpp")

IF(NOT LIBZMQ_HEADER_PATH)
    MESSAGE(FATAL_ERROR "libzmq development headers are required.")
ENDIF()

# libmsgpack
FIND_PATH(LIBMSGPACK_HEADER_PATH "msgpack.hpp")

IF(NOT LIBMSGPACK_HEADER_PATH)
    MESSAGE(FATAL_ERROR "libmsgpack development headers are required.")
ENDIF()

# liburiparser
FIND_PATH(LIBURIPARSER_HEADER_PATH "Uri.h" PATH_SUFFIXES "uriparser")

IF(NOT LIBURIPARSER_HEADER_PATH)
    MESSAGE(FATAL_ERROR "liburiparser development headers are required.")
ENDIF()

INCLUDE_DIRECTORIES(${LIBEV_HEADER_PATH})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/foreign/jsoncpp-0.5.0/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

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
    boost_thread
    boost_filesystem
    ev
    zmq
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
    boost_program_options)

ADD_DEPENDENCIES(yappi yappi-core)

ADD_SUBDIRECTORY(plugins)
ADD_SUBDIRECTORY(clients)
ADD_SUBDIRECTORY(toolset)

INSTALL(TARGETS yappi yappi-core
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime NAMELINK_SKIP
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME} COMPONENT development)
