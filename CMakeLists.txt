CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(Yappi)

FIND_PACKAGE(PkgConfig)

FIND_PACKAGE(Boost 1.34.1 REQUIRED COMPONENTS 
    thread 
    filesystem 
    program_options)

PKG_CHECK_MODULES(LIBCRYPTO REQUIRED libcrypto)
PKG_CHECK_MODULES(LIBUUID REQUIRED uuid)
PKG_CHECK_MODULES(LIBZMQ REQUIRED libzmq>=2.1.7)
PKG_CHECK_MODULES(LIBCURL REQUIRED libcurl)

# libev
FIND_PATH(LIBEV_INCLUDE_DIRS NAMES "ev++.h" PATH_SUFFIXES "ev" "libev")

IF(NOT LIBEV_INCLUDE_DIRS)
    MESSAGE(FATAL_ERROR "libev development headers are required.")
ENDIF()

# libmsgpack
FIND_PATH(LIBMSGPACK_INCLUDE_DIRS NAMES "msgpack.hpp" PATH_SUFFIXES "msgpack" "libmsgpack")

IF(NOT LIBMSGPACK_INCLUDE_DIRS)
    MESSAGE(FATAL_ERROR "libmsgpack development headers are required.")
ENDIF()

# liburiparser
FIND_PATH(LIBURIPARSER_INCLUDE_DIRS NAMES "uriparser/Uri.h")

IF(NOT LIBURIPARSER_INCLUDE_DIRS)
    MESSAGE(FATAL_ERROR "liburiparser development headers are required.")
ENDIF()

INCLUDE_DIRECTORIES(
    ${LIBCRYPTO_INCLUDE_DIRS}
    ${LIBUUID_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${LIBZMQ_INCLUDE_DIRS}
    ${LIBEV_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBURIPARSER_INCLUDE_DIRS})

LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})

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
    uuid
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
