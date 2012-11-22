CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(COCAINE-CORE)

SET(COCAINE_VERSION 0.10)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

CONFIGURE_FILE(
    "${PROJECT_SOURCE_DIR}/config.hpp.in"
    "${PROJECT_SOURCE_DIR}/include/cocaine/config.hpp")

FIND_PACKAGE(Boost 1.40.0 REQUIRED
    COMPONENTS
        filesystem
        program_options
        thread)

FIND_PACKAGE(OpenSSL REQUIRED)

INCLUDE(cmake/locate_library.cmake)

LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBZMQ "zmq.hpp" "zmq")
LOCATE_LIBRARY(LIBARCHIVE "archive.h" "archive")
LOCATE_LIBRARY(LIBLTDL "ltdl.h" "ltdl")

IF(NOT APPLE)
    LOCATE_LIBRARY(LIBUUID "uuid/uuid.h" "uuid")
    SET(LIBUUID_LIBRARY "uuid")
ENDIF()

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${LIBEV_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBZMQ_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS})

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBEV_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS}
    ${LIBZMQ_LIBRARY_DIRS}
    ${LIBARCHIVE_LIBRARY_DIRS})

INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/foreign/jsoncpp-0.6.0-rc2/include)
INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/include)

ADD_LIBRARY(json
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_value
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_reader
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_writer)

SET_TARGET_PROPERTIES(json PROPERTIES
    COMPILE_FLAGS -fPIC)

ADD_LIBRARY(cocaine-common SHARED
    src/auth
    src/context
    src/io
    src/isolates/process
    src/repository
    src/sinks/file
    src/sinks/stdio
    src/sinks/syslog
    src/storages/files)

TARGET_LINK_LIBRARIES(cocaine-common
    boost_filesystem-mt
    boost_system-mt
    boost_thread-mt
    crypto
    json
    ltdl
    msgpack
    zmq)

SET_TARGET_PROPERTIES(cocaine-common PROPERTIES
    VERSION 2
    COMPILE_FLAGS "-std=c++0x")

ADD_LIBRARY(cocaine-core SHARED
    src/app
    src/archive
    src/engine
    src/manifest
    src/profile
    src/session
    src/slave)

TARGET_LINK_LIBRARIES(cocaine-core
    archive
    cocaine-common
    ev
    ${LIBUUID_LIBRARY})

SET_TARGET_PROPERTIES(cocaine-core PROPERTIES
    VERSION 2
    COMPILE_FLAGS "-std=c++0x")

ADD_EXECUTABLE(cocaine-generic-worker
    src/generic-worker/worker
    src/generic-worker/main)

TARGET_LINK_LIBRARIES(cocaine-generic-worker
    boost_program_options-mt
    cocaine-core)

SET_TARGET_PROPERTIES(cocaine-generic-worker PROPERTIES
    COMPILE_FLAGS "-std=c++0x")

ADD_EXECUTABLE(cocained
    src/server/server
    src/server/main)

TARGET_LINK_LIBRARIES(cocained
    boost_program_options-mt
    cocaine-core)

SET_TARGET_PROPERTIES(cocained PROPERTIES
    COMPILE_FLAGS "-std=c++0x")

ADD_SUBDIRECTORY(tools)

INSTALL(
    TARGETS
        cocaine-common
        cocaine-core
        cocaine-generic-worker
        cocained
        json
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT developement)

INSTALL(
    DIRECTORY
        include/
        foreign/jsoncpp-0.6.0-rc2/include/
    DESTINATION include
    COMPONENT development)
