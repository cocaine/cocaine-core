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
