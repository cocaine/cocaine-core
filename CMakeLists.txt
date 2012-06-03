CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(Cocaine)

SET(COCAINE_VERSION 0.7)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

OPTION(DEALER "Build dealer library" ON)
OPTION(PYTHON "Build python module" ON)
OPTION(PERL "Build perl module" ON)
OPTION(JAVASCRIPT "Build javascript module" ON)
OPTION(MONGO "Build mongo module" ON)

CONFIGURE_FILE(
    "${PROJECT_SOURCE_DIR}/config.hpp.in"
    "${PROJECT_SOURCE_DIR}/include/cocaine/config.hpp")

FIND_PACKAGE(Boost 1.34.1 REQUIRED
    COMPONENTS
        filesystem
        program_options
        thread)

FIND_PACKAGE(OpenSSL REQUIRED)
FIND_PACKAGE(CURL REQUIRED)

INCLUDE(cmake/locate_library.cmake)

LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBZMQ "zmq.hpp" "zmq")

IF(NOT APPLE)
    LOCATE_LIBRARY(LIBCGROUP "libcgroup.h" "cgroup")
    LOCATE_LIBRARY(LIBUUID "uuid/uuid.h" "uuid")

    SET(LIBCGROUP_LIBRARY "cgroup")
    SET(LIBUUID_LIBRARY "uuid")

    ADD_DEFINITIONS("-DHAVE_CGROUPS")
ENDIF()

IF(Boost_MINOR_VERSION GREATER 34)
    SET(Boost_SYSTEM_LIBRARY "boost_system-mt")
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
INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/include)

#ADD_DEFINITIONS("-DBOOST_FILESYSTEM_VERSION=2")

ADD_LIBRARY(json
    foreign/jsoncpp-0.5.0/src/lib_json/json_value
    foreign/jsoncpp-0.5.0/src/lib_json/json_reader
    foreign/jsoncpp-0.5.0/src/lib_json/json_writer)

SET_TARGET_PROPERTIES(json PROPERTIES
    COMPILE_FLAGS -fPIC)

ADD_LIBRARY(cocaine-common SHARED
    src/auth
    src/context
    src/interfaces/storage
    src/loggers/syslog
    src/logging
    src/repository
    src/storages/files
    src/storages/void)

TARGET_LINK_LIBRARIES(cocaine-common
    boost_filesystem-mt
    boost_thread-mt
    crypto
    json
    ltdl
    zmq
    ${Boost_SYSTEM_LIBRARY}
    ${LIBCGROUP_LIBRARY}
    ${LIBUUID_LIBRARY})

SET_TARGET_PROPERTIES(cocaine-common PROPERTIES
    VERSION 1)

ADD_LIBRARY(cocaine-core SHARED
    src/drivers/base
    src/engine
    src/interfaces/plugin
    src/job
    src/manifest
    src/overseer
    src/slave)

TARGET_LINK_LIBRARIES(cocaine-core
    cocaine-common
    ev
    msgpack)

SET_TARGET_PROPERTIES(cocaine-core PROPERTIES
    VERSION 1)

ADD_EXECUTABLE(cocained
    src/server/server
    src/server/main)

ADD_EXECUTABLE(library-test
    tests/library_test)

TARGET_LINK_LIBRARIES(cocained
    boost_program_options-mt
    cocaine-core)

TARGET_LINK_LIBRARIES(library-test
    cocaine-core)

ADD_SUBDIRECTORY(modules)

INSTALL(
    TARGETS
        cocaine-common
        cocaine-core
        cocained
        json
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT developement)

INSTALL(
    DIRECTORY
        include/
        foreign/jsoncpp-0.5.0/include/
    DESTINATION include
    COMPONENT development)
