CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(COCAINE-CORE)

SET(COCAINE_VERSION 0.10)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

CONFIGURE_FILE(
    "${PROJECT_SOURCE_DIR}/config.hpp.in"
    "${PROJECT_SOURCE_DIR}/include/cocaine/config.hpp")

FIND_PACKAGE(Boost 1.40.0 REQUIRED
    COMPONENTS
        system
        filesystem
        program_options)

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

INCLUDE_DIRECTORIES(BEFORE
    ${PROJECT_SOURCE_DIR}/foreign/jsoncpp-0.6.0-rc2/include
    ${PROJECT_SOURCE_DIR}/include)

ADD_LIBRARY(json
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_value
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_reader
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_writer)

SET_TARGET_PROPERTIES(json PROPERTIES
    COMPILE_FLAGS -fPIC)

ADD_LIBRARY(cocaine-core SHARED
    src/actor
    src/api
    src/app
    src/archive
    src/auth
    src/context
    src/dispatch
    src/engine
    src/logging
    src/manifest
    src/profile
    src/repository
    src/session
    src/slave
    src/unique_id)

TARGET_LINK_LIBRARIES(cocaine-core
    archive
    boost_system-mt
    boost_filesystem-mt
    crypto
    ev
    json
    ltdl
    msgpack
    ${LIBUUID_LIBRARY})

SET_TARGET_PROPERTIES(cocaine-core PROPERTIES
    VERSION 2
    COMPILE_FLAGS "-std=c++0x")

ADD_LIBRARY(essentials MODULE
    src/essentials/isolates/process
    src/essentials/loggers/files
    src/essentials/loggers/stdout
    src/essentials/loggers/syslog
    src/essentials/services/locator
    src/essentials/services/logging
    src/essentials/services/node
    src/essentials/storages/files
    src/essentials/module)

TARGET_LINK_LIBRARIES(essentials
    cocaine-core
    zmq)

SET_TARGET_PROPERTIES(essentials PROPERTIES
    COMPILE_FLAGS "-std=c++0x"
    PREFIX ""
    SUFFIX ".cocaine-plugin")

ADD_EXECUTABLE(cocaine-runtime
    src/runtime/pid_file
    src/runtime/bootstrap)

TARGET_LINK_LIBRARIES(cocaine-runtime
    boost_program_options-mt
    cocaine-core)

SET_TARGET_PROPERTIES(cocaine-runtime PROPERTIES
    COMPILE_FLAGS "-std=c++0x")

ADD_SUBDIRECTORY(tools)

INSTALL(
    TARGETS
        cocaine-core
        cocaine-runtime
        json
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT developement)

INSTALL(
    TARGETS
        essentials
    LIBRARY DESTINATION lib/cocaine COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/
        foreign/jsoncpp-0.6.0-rc2/include/
    DESTINATION include
    COMPONENT development)
