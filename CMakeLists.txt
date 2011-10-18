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

INCLUDE(cmake/LocateLibrary.cmake)
INCLUDE(cmake/PrecompiledHeader.cmake)

LOCATE_LIBRARY(LIBZMQ "zmq.hpp" "zmq")
LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBURIPARSER "uriparser/Uri.h" "uriparser")
LOCATE_LIBRARY(LIBUUID "uuid/uuid.h" "uuid")

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

INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/foreign/libjson)
INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/foreign/jsoncpp-0.5.0/include)
INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/include)

IF(NOT APPLE)
    SET(UUID_LIBS uuid)
ENDIF()

ADD_LIBRARY(json
    foreign/jsoncpp-0.5.0/src/lib_json/json_value
    foreign/jsoncpp-0.5.0/src/lib_json/json_reader
    foreign/jsoncpp-0.5.0/src/lib_json/json_writer)

SET_TARGET_PROPERTIES(json PROPERTIES
    COMPILE_FLAGS -fPIC)

ADD_DEFINITIONS("-DBOOST_FILESYSTEM_VERSION=2")

ADD_LIBRARY(cocaine-core-config SHARED
    src/config)

SET_TARGET_PROPERTIES(cocaine-core-config PROPERTIES
    VERSION 1)

ADD_LIBRARY(cocaine-core-storage SHARED
    src/storage
    src/file_storage
    src/mongo_storage)

TARGET_LINK_LIBRARIES(cocaine-core-storage
    cocaine-core-config
    json
    ${BOOST_SYSTEM_LIBS}
    boost_filesystem-mt
    boost_thread-mt
    mongoclient)

SET_TARGET_PROPERTIES(cocaine-core-storage PROPERTIES
    VERSION 1)

ADD_LIBRARY(cocaine-core-security SHARED
    src/signatures
    src/digest)

TARGET_LINK_LIBRARIES(cocaine-core-security
    cocaine-core-storage
    crypto)

SET_TARGET_PROPERTIES(cocaine-core-security PROPERTIES
    VERSION 1)

ADD_LIBRARY(cocaine-core SHARED 
    src/core
    src/engine
    src/networking
    src/overseer
    src/registry
    src/thread)

TARGET_LINK_LIBRARIES(cocaine-core
    cocaine-core-config
    cocaine-core-security
    cocaine-core-storage
    json
    ${CMAKE_DL_LIBS}
    ${UUID_LIBS}
    boost_thread-mt
    zmq
    ev
    msgpack)

SET_TARGET_PROPERTIES(cocaine-core PROPERTIES
    VERSION 1)

ADD_EXECUTABLE(cocained
    src/main)

TARGET_LINK_LIBRARIES(cocained
    cocaine-core-config
    cocaine-core
    boost_program_options-mt)

ADD_SUBDIRECTORY(plugins)
ADD_SUBDIRECTORY(clients)
ADD_SUBDIRECTORY(toolset)

INSTALL(
    TARGETS
        json 
        cocaine-core-config
        cocaine-core-storage
        cocaine-core-security
        cocaine-core 
        cocained
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT developement)

INSTALL(DIRECTORY
        include/
        foreign/jsoncpp-0.5.0/include/
    DESTINATION include
    COMPONENT development)
