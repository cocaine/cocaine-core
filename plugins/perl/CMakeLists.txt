ADD_LIBRARY(cocaine-plugin-perl MODULE
    perl)

TARGET_LINK_LIBRARIES(cocaine-plugin-perl
    cocaine-core-config
    cocaine-core
    json
    ${BOOST_SYSTEM_LIBS}
    boost_filesystem-mt
    uriparser)

INSTALL(TARGETS cocaine-plugin-perl
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
