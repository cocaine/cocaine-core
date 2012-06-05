cmake_minimum_required(VERSION 2.6)

IF (PERL)
    EXECUTE_PROCESS(
        COMMAND perl -MExtUtils::Embed -e ccopts
        OUTPUT_VARIABLE PERL_CFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    EXECUTE_PROCESS(
        COMMAND perl -MExtUtils::Embed -e ldopts
        OUTPUT_VARIABLE PERL_LDFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    ADD_LIBRARY(perl MODULE
        perl)

    TARGET_LINK_LIBRARIES(perl
        cocaine-common)

    SET_TARGET_PROPERTIES(perl PROPERTIES
        COMPILE_FLAGS "${PERL_CFLAGS}"
        LINK_FLAGS "${PERL_LDFLAGS}"
        PREFIX ""
        SUFFIX ".cocaine-plugin")

    INSTALL(TARGETS perl
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF()
