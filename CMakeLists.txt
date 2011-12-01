CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(Cocaine)

FIND_PACKAGE(Boost 1.35.0 REQUIRED 
    COMPONENTS
        filesystem
        program_options
        system
        thread)

FIND_PACKAGE(OpenSSL REQUIRED)
FIND_PACKAGE(CURL REQUIRED)

INCLUDE(cmake/LocateLibrary.cmake)

LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBURIPARSER "uriparser/Uri.h" "uriparser")
LOCATE_LIBRARY(LIBZMQ "zmq.hpp" "zmq")

IF(NOT APPLE)
    LOCATE_LIBRARY(LIBUUID "uuid/uuid.h" "uuid")
ENDIF()

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${LIBEV_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBZMQ_INCLUDE_DIRS})

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBEV_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS}
    ${LIBZMQ_LIBRARY_DIRS})

INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/foreign/jsoncpp-0.5.0/include)
INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/foreign/http-parser)
INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/include)

ADD_DEFINITIONS("-DBOOST_FILESYSTEM_VERSION=2")

ADD_LIBRARY(json
    foreign/jsoncpp-0.5.0/src/lib_json/json_value
    foreign/jsoncpp-0.5.0/src/lib_json/json_reader
    foreign/jsoncpp-0.5.0/src/lib_json/json_writer)

SET_TARGET_PROPERTIES(json PROPERTIES
    COMPILE_FLAGS -fPIC)

ADD_LIBRARY(cocaine-common SHARED
    src/config
    src/security
    src/storages/base
    src/storages/files
    src/storages/mongo)

TARGET_LINK_LIBRARIES(cocaine-common
    boost_filesystem-mt
    boost_system-mt
    boost_thread-mt
    crypto
    json
    mongoclient
    ${LIBUUID_LIBRARIES})

SET_TARGET_PROPERTIES(cocaine-common PROPERTIES
    VERSION 1)

ADD_LIBRARY(cocaine-core SHARED 
    src/core
    src/downloads
    src/drivers/base
    src/drivers/fs
    src/drivers/server+zmq
    src/drivers/server+lsd
    src/drivers/timed+auto
    src/engine
    src/job
    src/lines
    src/overseer
    src/registry
    src/slaves/base
    src/slaves/process
    src/slaves/thread)

TARGET_LINK_LIBRARIES(cocaine-core
    cocaine-common
    curl
    ev
    ltdl
    msgpack
    zmq)

SET_TARGET_PROPERTIES(cocaine-core PROPERTIES
    VERSION 1)

ADD_EXECUTABLE(cocained
    src/main)

TARGET_LINK_LIBRARIES(cocained
    boost_program_options-mt
    cocaine-core)

ADD_SUBDIRECTORY(plugins)

INSTALL(
    TARGETS
        cocaine-common
        cocaine-core
        cocained
        json 
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT developement)

INSTALL(DIRECTORY
        include/
        foreign/jsoncpp-0.5.0/include/
    DESTINATION include
    COMPONENT development)
