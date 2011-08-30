CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(Yappi)

FIND_PACKAGE(OpenSSL REQUIRED)
FIND_PACKAGE(Boost 1.34.1 REQUIRED COMPONENTS 
    system
    thread 
    filesystem 
    program_options)

FUNCTION(LOCATE_LIBRARY VARIABLE TYPE HEADER LIBRARY)
    IF(${VARIABLE}_INCLUDE_DIRS AND ${VARIABLE}_LIBRARIES)
        RETURN()
    ENDIF()

    FIND_PATH(${VARIABLE}_INCLUDE_DIRS NAMES ${HEADER} PATH_SUFFIXES ${LIBRARY} lib${LIBRARY})
    FIND_LIBRARY(${VARIABLE}_LIBRARIES NAMES ${LIBRARY} PATH_SUFFIXES ${LIBRARY} lib${LIBRARY})
    GET_FILENAME_COMPONENT(${VARIABLE}_LIBRARY_DIRS ${${VARIABLE}_LIBRARIES} PATH)
    STRING(TOLOWER ${VARIABLE} LIBRARY_NAME)

    IF(NOT ${VARIABLE}_INCLUDE_DIRS OR NOT ${VARIABLE}_LIBRARIES)
        MESSAGE(${TYPE} "${LIBRARY_NAME} development files are required.")
    ELSE()
        MESSAGE(STATUS "Found ${LIBRARY_NAME}: ${${VARIABLE}_LIBRARIES}")
    ENDIF()
ENDFUNCTION()

LOCATE_LIBRARY(LIBZMQ FATAL_ERROR "zmq.hpp" "zmq")
LOCATE_LIBRARY(LIBEV FATAL_ERROR "ev++.h" "ev")
LOCATE_LIBRARY(LIBMSGPACK FATAL_ERROR "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBURIPARSER FATAL_ERROR "uriparser/Uri.h" "uriparser")

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${LIBZMQ_INCLUDE_DIRS}
    ${LIBEV_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBURIPARSER_INCLUDE_DIRS})

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/foreign/jsoncpp-0.5.0/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

ADD_LIBRARY(json
    foreign/jsoncpp-0.5.0/src/lib_json/json_value
    foreign/jsoncpp-0.5.0/src/lib_json/json_reader
    foreign/jsoncpp-0.5.0/src/lib_json/json_writer)

SET_TARGET_PROPERTIES(json PROPERTIES
    COMPILE_FLAGS -fPIC)

ADD_LIBRARY(yappi-core SHARED 
    src/core
    src/engine
    src/registry
    src/sockets
    src/file_storage
    src/security)

TARGET_LINK_LIBRARIES(yappi-core
    json
    crypto
    dl
    boost_system-mt
    boost_thread-mt
    boost_filesystem-mt
    zmq
    ev
    msgpack
    uriparser)

SET(YAPPI_HEADERS
    include/plugin.hpp
    include/uri.hpp
    include/digest.hpp)

SET_TARGET_PROPERTIES(yappi-core PROPERTIES
    VERSION 1
    COMPILE_DEFINITIONS "BOOST_FILESYSTEM_VERSION=2"
    PUBLIC_HEADER "${YAPPI_HEADERS}")

ADD_DEPENDENCIES(yappi-core json)

ADD_EXECUTABLE(yappi src/main)

TARGET_LINK_LIBRARIES(yappi
    yappi-core
    boost_program_options-mt)

ADD_DEPENDENCIES(yappi yappi-core)

ADD_SUBDIRECTORY(plugins)
ADD_SUBDIRECTORY(clients)
ADD_SUBDIRECTORY(toolset)

INSTALL(TARGETS yappi yappi-core
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime NAMELINK_SKIP
    PUBLIC_HEADER DESTINATION include/yappi COMPONENT development)
