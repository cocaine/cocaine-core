list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../foreign/modules")

SET(PROJECT cocaine-core-tests)

PROJECT(${PROJECT})

# Benchmarks
IF(COCAINE_ALLOW_BENCHMARKS)
    INCLUDE_DIRECTORIES(
        foreign/celero/include)

    ADD_SUBDIRECTORY(
        foreign/celero)

    UNSET(CELERO_RUN_EXAMPLE_ON_BUILD)
    UNSET(CELERO_CELERO_ENABLE_TESTS)
    UNSET(CELERO_COMPILE_DYNAMIC_LIBRARIES)

    ADD_EXECUTABLE(cocaine-benchmark
        benchmark.cpp)

    TARGET_LINK_LIBRARIES(cocaine-benchmark
        celero
        cocaine-core)

    SET_TARGET_PROPERTIES(cocaine-benchmark PROPERTIES
    COMPILE_FLAGS "-std=c++0x -W -Wall -Werror -pedantic")
ENDIF()

# Unit tests
IF(COCAINE_ALLOW_TESTS AND
    NOT (CMAKE_VERSION VERSION_LESS 2.8.5 OR (DEFINED GCC_COMPILER_VERSION AND GCC_COMPILER_VERSION VERSION_LESS 4.8)))
    INCLUDE(DownloadGoogleTesting)
    FIND_PACKAGE(Threads)

    DOWNLOAD_GOOGLE_TESTING()

    INCLUDE_DIRECTORIES(
        SYSTEM ${GTEST_INCLUDE_DIR}
        SYSTEM ${GMOCK_INCLUDE_DIR})

    LINK_DIRECTORIES(
        ${GMOCK_BINARY_DIR}
        ${GTEST_BINARY_DIR})

    INCLUDE_DIRECTORIES(
        ${CMAKE_CURRENT_SOURCE_DIR}/../include)

    ADD_EXECUTABLE(cocaine-core-unit
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/protocol.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/header.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/header_table.cpp)

    ADD_DEPENDENCIES(cocaine-core-unit googlemock)

    TARGET_LINK_LIBRARIES(cocaine-core-unit
        ${CMAKE_THREAD_LIBS_INIT}
        cocaine-core
        cocaine-io-util
        gmock
        gtest
        gtest_main)

    SET_TARGET_PROPERTIES(cocaine-core-unit PROPERTIES
        COMPILE_FLAGS "-std=c++0x -W -Wall -Werror -pedantic")
ENDIF()
