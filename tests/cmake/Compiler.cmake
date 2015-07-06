IF(CMAKE_COMPILER_IS_GNUCXX)
    EXEC_PROGRAM(${CMAKE_CXX_COMPILER}
        ARGS --version
        OUTPUT_VARIABLE _compiler_output
    )

    STRING(REGEX REPLACE ".* ([0-9]\\.[0-9]\\.[0-9]).*" "\\1"
        GCC_COMPILER_VERSION ${_compiler_output}
    )
ENDIF()
