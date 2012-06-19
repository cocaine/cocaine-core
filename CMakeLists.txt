CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(Cocaine)

SET(COCAINE_VERSION 0.8)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

OPTION(DEALER "Build Dealer library" ON)
OPTION(PYTHON "Build Python plugin" ON)
OPTION(PERL "Build Perl plugin" ON)
OPTION(JAVASCRIPT "Build Javascript plugin" ON)
OPTION(MONGO "Build MongoDB plugin" ON)

IF(NOT APPLE)
    OPTION(ELLIPTICS "Build Elliptics plugin" ON)
ELSE()
    OPTION(ELLIPTICS "Build Elliptics plugin" OFF)
ENDIF()

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

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
LOCATE_LIBRARY(LIBARCHIVE "archive.h" "archive")

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

#ADD_DEFINITIONS("-DBOOST_FILESYSTEM_VERSION=2")

ADD_LIBRARY(json
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_value
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_reader
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_writer)

SET_TARGET_PROPERTIES(json PROPERTIES
    COMPILE_FLAGS -fPIC)

ADD_LIBRARY(cocaine-common SHARED
    src/auth
    src/context
    src/loggers/syslog
    src/logging
    src/repository
    src/storages/files)

TARGET_LINK_LIBRARIES(cocaine-common
    boost_filesystem-mt
    ${Boost_SYSTEM_LIBRARY}
    boost_thread-mt
    ${LIBCGROUP_LIBRARY}
    crypto
    json
    ltdl
    msgpack
    ${LIBUUID_LIBRARY}
    zmq)

SET_TARGET_PROPERTIES(cocaine-common PROPERTIES
    VERSION 1)

ADD_LIBRARY(cocaine-core SHARED
    src/app
    src/engine
    src/job
    src/manifest
    src/master
    src/package
    src/policy)

TARGET_LINK_LIBRARIES(cocaine-core
    archive
    cocaine-common
    ev)

SET_TARGET_PROPERTIES(cocaine-core PROPERTIES
    VERSION 1)

ADD_EXECUTABLE(cocaine-slave
    src/slave/slave
    src/slave/main)

ADD_EXECUTABLE(cocained
    src/server/server
    src/server/main)

ADD_EXECUTABLE(library-test
    tests/library_test.cpp)

TARGET_LINK_LIBRARIES(cocaine-slave
    boost_program_options-mt
    cocaine-core)

TARGET_LINK_LIBRARIES(cocained
    boost_program_options-mt
    cocaine-core)

TARGET_LINK_LIBRARIES(library-test
    cocaine-core)

IF(DEALER)
    FILE(GLOB DEALER_SOURCES
        "src/dealer/*.cpp")

    ADD_LIBRARY(cocaine-dealer SHARED
        ${DEALER_SOURCES})

    TARGET_LINK_LIBRARIES(cocaine-dealer
        boost_thread-mt
        crypto
        curl
        json
        msgpack
        zmq
        eblob_cpp
        eblob
        ${Boost_SYSTEM_LIBRARY}
        ${LIBUUID_LIBRARY})

    SET_TARGET_PROPERTIES(cocaine-dealer PROPERTIES
        VERSION 1)

    ADD_EXECUTABLE(dealer_test_app
        tests/dealer_test_app.cpp)

    TARGET_LINK_LIBRARIES(dealer_test_app
        boost_program_options-mt
        cocaine-dealer
        boost_filesystem-mt
        zmq)

    INSTALL(
        TARGETS
            cocaine-dealer
        RUNTIME DESTINATION bin COMPONENT runtime
        LIBRARY DESTINATION lib COMPONENT runtime
        ARCHIVE DESTINATION lib COMPONENT developement)

ENDIF()

ADD_SUBDIRECTORY(plugins)
ADD_SUBDIRECTORY(tools)

INSTALL(
    TARGETS
        cocaine-common
        cocaine-core
        cocaine-slave
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
