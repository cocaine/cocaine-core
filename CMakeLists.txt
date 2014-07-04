CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(COCAINE-CORE)

SET(COCAINE_VERSION_MAJOR   0)
SET(COCAINE_VERSION_MINOR   12)
SET(COCAINE_VERSION_RELEASE 0)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

IF(NOT APPLE)
    OPTION(COCAINE_ALLOW_CGROUPS "Build CGroups support for Process Isolate" ON)
ELSE()
    OPTION(COCAINE_ALLOW_CGROUPS "Build CGroups support for Process Isolate" OFF)
ENDIF()

OPTION(COCAINE_ALLOW_RAFT "Build Raft support for Runtime" ON)

INCLUDE(cmake/locate_library.cmake)

LOCATE_LIBRARY(LIBARCHIVE "archive.h" "archive")

IF(NOT APPLE)
    LOCATE_LIBRARY(LIBBFD "bfd.h" "bfd")
    SET(LIBBFD_LIBRARY "bfd" "dl")
ENDIF()

FIND_PACKAGE(Boost 1.40.0
    REQUIRED COMPONENTS
        filesystem
        program_options
        system
        thread)

IF(COCAINE_ALLOW_CGROUPS)
    LOCATE_LIBRARY(LIBCGROUP "libcgroup.h" "cgroup")
    SET(LIBCGROUP_LIBRARY "cgroup")
ENDIF()

LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(LIBLTDL "ltdl.h" "ltdl")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBMHASH "mhash.h" "mhash")

IF(NOT APPLE)
    LOCATE_LIBRARY(LIBUUID "uuid/uuid.h" "uuid")
    SET(LIBUUID_LIBRARY "uuid")
ENDIF()

CONFIGURE_FILE(
    "${PROJECT_SOURCE_DIR}/config.hpp.in"
    "${PROJECT_SOURCE_DIR}/include/cocaine/config.hpp")

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${LIBCGROUP_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${LIBEV_INCLUDE_DIRS}
    ${LIBMHASH_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${LIBLTDL_INCLUDE_DIRS})

INCLUDE_DIRECTORIES(BEFORE
    ${PROJECT_SOURCE_DIR}/foreign/backward-cpp
    ${PROJECT_SOURCE_DIR}/foreign/blackhole/src
    ${PROJECT_SOURCE_DIR}/foreign/rapidjson/include
    ${PROJECT_SOURCE_DIR}/include)

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBCGROUP_LIBRARY_DIRS}
    ${LIBEV_LIBRARY_DIRS}
    ${LIBMHASH_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS}
    ${LIBARCHIVE_LIBRARY_DIRS}
    ${LIBLTDL_LIBRARY_DIRS})

IF(COCAINE_ALLOW_RAFT)
    ADD_LIBRARY(cocaine-raft STATIC
        src/raft
        src/services/counter)

    SET_TARGET_PROPERTIES(cocaine-raft PROPERTIES
        COMPILE_FLAGS "-std=c++0x -W -Wall -Werror -pedantic -fPIC")

    SET(LIBCOCAINE_RAFT cocaine-raft)
ENDIF()

ADD_LIBRARY(cocaine-core SHARED
    src/actor
    src/api
    src/chamber
    src/context
    src/crypto
    src/dispatch
    src/dynamic
    src/engine
    src/essentials
    src/gateways/adhoc
    src/isolates/process.cpp
    src/isolates/process/archive
    src/isolates/process/spooler
    src/locator
    src/logging
    src/repository
    src/services/logging
    src/services/node.cpp
    src/services/node/app
    src/services/node/engine
    src/services/node/manifest
    src/services/node/profile
    src/services/node/queue
    src/services/node/session
    src/services/node/slave
    src/services/storage
    src/session
    src/storages/files
    src/unique_id)

TARGET_LINK_LIBRARIES(cocaine-core
    archive
    ${Boost_LIBRARIES}
    ${LIBCGROUP_LIBRARY}
    ${LIBCOCAINE_RAFT}
    ev
    ltdl
    mhash
    msgpack
    ${LIBUUID_LIBRARY})

SET_TARGET_PROPERTIES(cocaine-core PROPERTIES
    VERSION 2)

ADD_EXECUTABLE(cocaine-runtime
    src/runtime/pid_file
    src/runtime/runtime)

TARGET_LINK_LIBRARIES(cocaine-runtime
    ${LIBBFD_LIBRARY}
    ${Boost_LIBRARIES}
    cocaine-core)

SET_TARGET_PROPERTIES(cocaine-core cocaine-runtime PROPERTIES
    COMPILE_FLAGS "-std=c++0x -W -Wall -Werror -pedantic")

IF(NOT COCAINE_LIBDIR)
    SET(COCAINE_LIBDIR lib)
ENDIF()

INSTALL(
    TARGETS
        cocaine-core
        cocaine-runtime
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION ${COCAINE_LIBDIR} COMPONENT runtime
    ARCHIVE DESTINATION ${COCAINE_LIBDIR} COMPONENT development)

INSTALL(
    DIRECTORY
        include/
    DESTINATION include
    COMPONENT development)
