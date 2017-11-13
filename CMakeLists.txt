CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(COCAINE-CORE)

SET(COCAINE_VERSION_MAJOR   0)
SET(COCAINE_VERSION_MINOR   12)
SET(COCAINE_VERSION_RELEASE 14)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

IF(APPLE)
    SET(CMAKE_MACOSX_RPATH ON)
ENDIF()

OPTION(COCAINE_ALLOW_TESTS "Build Tests" OFF)
OPTION(COCAINE_ALLOW_BENCHMARKS "Build Benchmarking Tools" OFF)

# Import our CMake modules.
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/foreign/modules )
INCLUDE(locate_library)
INCLUDE(AddCXXCompilerFlag)

IF(NOT APPLE)
    LOCATE_LIBRARY(LIBBFD "bfd.h" "bfd")
    SET(LIBBFD_LIBRARY "bfd" "dl")
ENDIF()

FIND_PACKAGE(Boost 1.46.0 REQUIRED COMPONENTS
    filesystem
    program_options
    system
    thread)

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

INCLUDE_DIRECTORIES(SYSTEM
    ${Boost_INCLUDE_DIRS}
    ${LIBMHASH_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBLTDL_INCLUDE_DIRS}
    # Bundled third-party libraries.
    ${PROJECT_SOURCE_DIR}/foreign/asio/asio/include
    ${PROJECT_SOURCE_DIR}/foreign/backward-cpp
    ${PROJECT_SOURCE_DIR}/foreign/rapidjson/include)

INCLUDE_DIRECTORIES(BEFORE
    ${PROJECT_SOURCE_DIR}/include)

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBMHASH_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS}
    ${LIBLTDL_LIBRARY_DIRS})

ADD_LIBRARY(cocaine-io-util SHARED
    src/encoder.cpp
    src/errors.cpp
    src/header.cpp
    src/trace.cpp)

ADD_LIBRARY(cocaine-core SHARED
    src/actor.cpp
    src/api/authentication.cpp
    src/api/authorization.cpp
    src/api/storage.cpp
    src/api/unicorn.cpp
    src/auth/uid.cpp
    src/authorization/event.cpp
    src/authorization/storage.cpp
    src/authorization/unicorn.cpp
    src/chamber.cpp
    src/cluster/multicast.cpp
    src/cluster/predefine.cpp
    src/context.cpp
    src/context/config.cpp
    src/context/mapper.cpp
    src/crypto.cpp
    src/decoder.cpp
    src/defaults.cpp
    src/dispatch.cpp
    src/dynamic.cpp
    src/engine.cpp
    src/essentials.cpp
    src/executor/asio.cpp
    src/gateway/adhoc.cpp
    src/logging.cpp
    src/repository.cpp
    src/service/locator.cpp
    src/service/locator/routing.cpp
    src/service/logging.cpp
    src/service/storage.cpp
    src/session.cpp
    src/signal.cpp
    src/storage/files.cpp
    src/trace/logger.cpp
    src/unicorn/value.cpp
    src/unique_id.cpp
)

TARGET_LINK_LIBRARIES(cocaine-io-util
    blackhole
    ${Boost_LIBRARIES})

TARGET_LINK_LIBRARIES(cocaine-core
    cocaine-io-util
    ${Boost_LIBRARIES}
    ${LIBBFD_LIBRARY}
    ltdl
    mhash
    msgpack
    blackhole
    metrics
    ${LIBUUID_LIBRARY})

SET_TARGET_PROPERTIES(cocaine-io-util PROPERTIES
    VERSION 3)

SET_TARGET_PROPERTIES(cocaine-core PROPERTIES
    VERSION 3)

IF (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    SET_TARGET_PROPERTIES(cocaine-core PROPERTIES
        LINK_FLAGS -Wl,--version-script=${PROJECT_SOURCE_DIR}/libcocaine-core3.version)
ENDIF()

ADD_EXECUTABLE(cocaine-runtime
    src/runtime/logging.cpp
    src/runtime/pid_file.cpp
    src/runtime/runtime.cpp)

TARGET_LINK_LIBRARIES(cocaine-runtime
    ${Boost_LIBRARIES}
    blackhole
    cocaine-core)

# Try and enable C++11.
ADD_CXX_COMPILER_FLAG(-std=c++11)

IF(NOT HAVE_CXX_FLAG_STD_CXX11)
  ADD_CXX_COMPILER_FLAG(-std=c++0x)
ENDIF()

# Turn compiler warnings.
ADD_CXX_COMPILER_FLAG(-Wall)
ADD_CXX_COMPILER_FLAG(-Wextra)
ADD_CXX_COMPILER_FLAG(-Werror RELEASE)
ADD_CXX_COMPILER_FLAG(-Woverloaded-virtual)
ADD_CXX_COMPILER_FLAG(-pedantic)
ADD_CXX_COMPILER_FLAG(-pedantic-errors)

IF(COCAINE_ALLOW_TESTS OR COCAINE_ALLOW_BENCHMARKS)
    ADD_CUSTOM_TARGET(unittest
        COMMAND ${CMAKE_BINARY_DIR}/tests/cocaine-core-unit
        DEPENDS cocaine-core-unit
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    ADD_SUBDIRECTORY(tests)
ENDIF()

IF(NOT COCAINE_LIBDIR)
    SET(COCAINE_LIBDIR lib)
ENDIF()

INSTALL(
    TARGETS
        cocaine-io-util
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
