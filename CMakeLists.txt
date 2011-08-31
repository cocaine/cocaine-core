CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(Yappi)

FIND_PACKAGE(Boost 1.34.1 REQUIRED COMPONENTS
    thread 
    filesystem 
    program_options)

IF(Boost_MINOR_VERSION GREATER 34)
    SET(BOOST_SYSTEM_LIBS "boost_system-mt")
ENDIF()

FIND_PACKAGE(OpenSSL REQUIRED)
FIND_PACKAGE(CURL REQUIRED)

INCLUDE(LocateLibrary.cmake)

LOCATE_LIBRARY(LIBZMQ "zmq.hpp" "zmq")
LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBURIPARSER "uriparser/Uri.h" "uriparser")

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${LIBZMQ_INCLUDE_DIRS}
    ${LIBEV_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS})

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBZMQ_LIBRARY_DIRS}
    ${LIBEV_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS})

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/foreign/jsoncpp-0.5.0/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

IF(NOT APPLE)
    SET(UUID_LIBS uuid)
ENDIF()

ADD_LIBRARY(json
    foreign/jsoncpp-0.5.0/src/lib_json/json_value
    foreign/jsoncpp-0.5.0/src/lib_json/json_reader
    foreign/jsoncpp-0.5.0/src/lib_json/json_writer)

SET_TARGET_PROPERTIES(json PROPERTIES
    COMPILE_FLAGS -fPIC)

ADD_LIBRARY(yappi-core-security SHARED
    src/security
    src/digest)

TARGET_LINK_LIBRARIES(yappi-core-security
    crypto
    ${UUID_LIBS}
    ${BOOST_SYSTEM_LIBS}
    boost_filesystem-mt)

SET_TARGET_PROPERTIES(yappi-core-security PROPERTIES
    VERSION 1
    PUBLIC_HEADER "include/digest.hpp")

ADD_LIBRARY(yappi-core-storage SHARED
    src/file_storage)

TARGET_LINK_LIBRARIES(yappi-core-storage
    json
    ${UUID_LIBS}
    ${BOOST_SYSTEM_LIBS}
    boost_filesystem-mt)

SET_TARGET_PROPERTIES(yappi-core-storage PROPERTIES
    VERSION 1)

ADD_LIBRARY(yappi-core SHARED 
    src/core
    src/registry
    src/networking
    src/engine)

TARGET_LINK_LIBRARIES(yappi-core
    json
    yappi-core-security
    yappi-core-storage
    ${CMAKE_DL_LIBS}
    ${UUID_LIBS}
    boost_thread-mt
    zmq
    ev
    msgpack)

SET_TARGET_PROPERTIES(yappi-core PROPERTIES
    VERSION 1
    PUBLIC_HEADER "include/plugin.hpp;include/uri.hpp")

SET_TARGET_PROPERTIES(yappi-core yappi-core-security yappi-core-storage PROPERTIES
    COMPILE_DEFINITIONS "BOOST_FILESYSTEM_VERSION=2")

ADD_DEPENDENCIES(yappi-core
    json
    yappi-core-security
    yappi-core-storage)

ADD_EXECUTABLE(yappi src/main)

TARGET_LINK_LIBRARIES(yappi
    yappi-core
    boost_program_options-mt)

ADD_DEPENDENCIES(yappi yappi-core)

ADD_SUBDIRECTORY(plugins)
ADD_SUBDIRECTORY(clients)
ADD_SUBDIRECTORY(toolset)

INSTALL(TARGETS yappi yappi-core yappi-core-security yappi-core-storage
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime NAMELINK_SKIP
    PUBLIC_HEADER DESTINATION include/yappi COMPONENT development)
