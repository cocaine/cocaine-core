cmake_minimum_required(VERSION 2.6)

EXECUTE_PROCESS(
    COMMAND perl -MExtUtils::Embed -e ccopts
    OUTPUT_VARIABLE PERL_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

EXECUTE_PROCESS(
    COMMAND perl -MExtUtils::Embed -e ldopts
    OUTPUT_VARIABLE PERL_LDFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

ADD_LIBRARY(cocaine-plugin-perl MODULE
    perl)

TARGET_LINK_LIBRARIES(cocaine-plugin-perl
    cocaine-common)

SET_TARGET_PROPERTIES(cocaine-plugin-perl PROPERTIES
    COMPILE_FLAGS "${PERL_CFLAGS}"
    LINK_FLAGS "${PERL_LDFLAGS}")

INSTALL(TARGETS cocaine-plugin-perl
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
