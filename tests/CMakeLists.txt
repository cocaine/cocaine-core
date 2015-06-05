LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
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
IF(COCAINE_ALLOW_TESTS)
    INCLUDE(DownloadGoogleTesting)
    download_google_testing()

    INCLUDE_DIRECTORIES(
        SYSTEM ${GTEST_INCLUDE_DIR}
        SYSTEM ${GMOCK_INCLUDE_DIR})

    LINK_DIRECTORIES(${GTEST_BINARY_DIR} ${GMOCK_BINARY_DIR})

    INCLUDE_DIRECTORIES(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

    ADD_EXECUTABLE(cocaine-core-unit
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/header_table.cpp)

    ADD_DEPENDENCIES(cocaine-core-unit googlemock)

    TARGET_LINK_LIBRARIES(cocaine-core-unit
        cocaine-core
        gmock
        gtest)

    SET_TARGET_PROPERTIES(cocaine-core-unit PROPERTIES
    COMPILE_FLAGS "-std=c++0x -W -Wall -Werror -pedantic")
ENDIF()
