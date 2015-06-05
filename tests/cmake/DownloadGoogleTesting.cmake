INCLUDE(Compiler)
INCLUDE(ExternalProject)

FUNCTION(DOWNLOAD_GOOGLE_TESTING)
    # Use gmock 1.6 for ancient compilers and systems.
    IF(CMAKE_VERSION VERSION_LESS 2.8.5 OR (DEFINED GCC_COMPILER_VERSION AND GCC_COMPILER_VERSION VERSION_LESS 4.8))
        # Hack for cmake (< 2.8.5), which cannot extract zip archives.
        # Also gmock doesn't compile on GCC (< 4.8).
        SET(GMOCK_URL "https://s3-us-west-2.amazonaws.com/blackhole.testing/gmock-1.6.0.tar.gz")
    ELSE()
        SET(GMOCK_URL "https://googlemock.googlecode.com/files/gmock-1.7.0.zip")
        SET(GMOCK_URL_MD5 073b984d8798ea1594f5e44d85b20d66)
    ENDIF()

    SET_DIRECTORY_PROPERTIES(properties EP_PREFIX "${CMAKE_BINARY_DIR}/foreign")
    ExternalProject_ADD(googlemock
        URL ${GMOCK_URL}
        SOURCE_DIR "${CMAKE_BINARY_DIR}/foreign/gmock"
        CMAKE_ARGS "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}" "-DCMAKE_CXX_FLAGS=-fPIC"
        INSTALL_COMMAND ""
    )
    ExternalProject_GET_PROPERTY(googlemock SOURCE_DIR)
    ExternalProject_GET_PROPERTY(googlemock BINARY_DIR)

    SET(GTEST_INCLUDE_DIR ${SOURCE_DIR}/gtest/include PARENT_SCOPE)
    SET(GMOCK_INCLUDE_DIR ${SOURCE_DIR}/include PARENT_SCOPE)
    SET(GTEST_BINARY_DIR ${BINARY_DIR}/gtest PARENT_SCOPE)
    SET(GMOCK_BINARY_DIR ${BINARY_DIR} PARENT_SCOPE)
ENDFUNCTION()
