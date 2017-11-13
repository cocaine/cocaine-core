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

    ADD_SUBDIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/foreign/.googletest EXCLUDE_FROM_ALL)

    ENABLE_TESTING()
    download_google_testing()

    INCLUDE_DIRECTORIES(SYSTEM foreign/.googletest/googletest/include)

    INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../include)

    ADD_EXECUTABLE(cocaine-core-tests
        unit/context.cpp
        unit/format.cpp
        unit/protocol.cpp
        unit/header.cpp
        unit/header_table.cpp
        unit/lexical_cast.cpp
        unit/uuid.cpp)

    TARGET_LINK_LIBRARIES(cocaine-core-tests
        ${CMAKE_THREAD_LIBS_INIT}
        cocaine-core
        cocaine-io-util
        gmock
        gtest
        gtest_main)

    SET_TARGET_PROPERTIES(cocaine-core-tests PROPERTIES
        COMPILE_FLAGS "-std=c++0x -W -Wall -Werror -pedantic")
ENDIF()
