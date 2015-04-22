CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(COCAINE-CORE)

if(APPLE)
    set(CMAKE_MACOSX_RPATH ON)
endif()

SET(COCAINE_VERSION_MAJOR   0)
SET(COCAINE_VERSION_MINOR   12)
SET(COCAINE_VERSION_RELEASE 1)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

IF(APPLE)
    SET(CMAKE_MACOSX_RPATH ON)
ENDIF()

IF(NOT APPLE)
    OPTION(COCAINE_ALLOW_CGROUPS "Build CGroups support for Process Isolate" ON)
ELSE()
    OPTION(COCAINE_ALLOW_CGROUPS "Build CGroups support for Process Isolate" OFF)
ENDIF()

OPTION(COCAINE_ALLOW_TESTS "Build Tests" OFF)
OPTION(COCAINE_ALLOW_BENCHMARKS "Build benchmarking tools" OFF)

# Import our CMake modules
INCLUDE(cmake/locate_library.cmake)
INCLUDE(foreign/modules/AddCXXCompilerFlag.cmake)

LOCATE_LIBRARY(LIBARCHIVE "archive.h" "archive")

IF(NOT APPLE)
    LOCATE_LIBRARY(LIBBFD "bfd.h" "bfd")
    SET(LIBBFD_LIBRARY "bfd" "dl")
ENDIF()

FIND_PACKAGE(Boost 1.46.0
    REQUIRED COMPONENTS
        filesystem
        program_options
        system
        thread)

IF(COCAINE_ALLOW_CGROUPS)
    LOCATE_LIBRARY(LIBCGROUP "libcgroup.h" "cgroup")
    SET(LIBCGROUP_LIBRARY "cgroup")
ENDIF()

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
    ${LIBMHASH_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${LIBLTDL_INCLUDE_DIRS})

INCLUDE_DIRECTORIES(BEFORE
    SYSTEM ${PROJECT_SOURCE_DIR}/foreign/backward-cpp
    ${PROJECT_SOURCE_DIR}/foreign/blackhole/src
    ${PROJECT_SOURCE_DIR}/foreign/rapidjson/include
    ${PROJECT_SOURCE_DIR}/foreign/asio/asio/include
    ${PROJECT_SOURCE_DIR}/foreign/hpack-headers/include
    ${PROJECT_SOURCE_DIR}/include)

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBCGROUP_LIBRARY_DIRS}
    ${LIBMHASH_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS}
    ${LIBARCHIVE_LIBRARY_DIRS}
    ${LIBLTDL_LIBRARY_DIRS})

ADD_LIBRARY(cocaine-core SHARED
    ${HEADERS}
    src/actor.cpp
    src/api.cpp
    src/chamber.cpp
    src/cluster/multicast.cpp
    src/cluster/predefine.cpp
    src/context.cpp
    src/context/config.cpp
    src/context/mapper.cpp
    src/crypto.cpp
    src/defaults.cpp
    src/dispatch.cpp
    src/dynamic.cpp
    src/engine.cpp
    src/errors.cpp
    src/essentials.cpp
    src/gateway/adhoc.cpp
    src/isolate/process.cpp
    src/isolate/process/archive.cpp
    src/isolate/process/spooler.cpp
    src/logging.cpp
    src/repository.cpp
    src/service/locator.cpp
    src/service/locator/routing.cpp
    src/service/logging.cpp

    src/service/node.cpp
    src/service/node/actor.cpp
    src/service/node/app.cpp
    src/service/node/app/stats.cpp
    src/service/node/balancing/load.cpp
    src/service/node/dispatch/client.cpp
    src/service/node/dispatch/worker.cpp
    src/service/node/overseer.cpp
    src/service/node/slave.cpp
    src/service/node/slave/channel.cpp
    src/service/node/slave/control.cpp
    src/service/node/slave/error.cpp
    src/service/node/slave/fetcher.cpp
    src/service/node/slave/state/active.cpp
    src/service/node/slave/state/handshaking.cpp
    src/service/node/slave/state/sealing.cpp
    src/service/node/slave/state/spawning.cpp
    src/service/node/slave/state/state.cpp
    src/service/node/slave/state/stopped.cpp
    src/service/node/slave/state/terminating.cpp

    src/service/node/manifest.cpp
    src/service/node/profile.cpp
    src/service/storage.cpp
    src/session.cpp
    src/storage/files.cpp
    src/unique_id.cpp)

ADD_SUBDIRECTORY(foreign/hpack-headers)

TARGET_LINK_LIBRARIES(cocaine-core
    hpack-headers-static
    archive
    ${Boost_LIBRARIES}
    ${LIBCGROUP_LIBRARY}
    ltdl
    mhash
    msgpack
    thrift
    ${LIBUUID_LIBRARY})

SET_TARGET_PROPERTIES(cocaine-core PROPERTIES
    VERSION 3)

ADD_EXECUTABLE(cocaine-runtime
    src/runtime/logging.cpp
    src/runtime/pid_file.cpp
    src/runtime/runtime.cpp)

TARGET_LINK_LIBRARIES(cocaine-runtime
    stdc++
    ${LIBBFD_LIBRARY}
    ${Boost_LIBRARIES}
    cocaine-core)

# Try and enable C++11.
add_cxx_compiler_flag(-std=c++11)
IF(NOT HAVE_CXX_FLAG_STD_CXX11)
  add_cxx_compiler_flag(-std=c++0x)
ENDIF()

# Turn compiler warnings.
add_cxx_compiler_flag(-Wall)
add_cxx_compiler_flag(-Wextra)
add_cxx_compiler_flag(-Werror RELEASE)
add_cxx_compiler_flag(-Woverloaded-virtual)
add_cxx_compiler_flag(-pedantic)
add_cxx_compiler_flag(-pedantic-errors)

IF(COCAINE_ALLOW_TESTS OR COCAINE_ALLOW_BENCHMARKS)
    ADD_SUBDIRECTORY(tests)
ENDIF()

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
    COMPONENT development
    PATTERN "detail" EXCLUDE)
