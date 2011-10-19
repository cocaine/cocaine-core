LOCATE_LIBRARY(LIBV8 "v8.h" "v8")

ADD_LIBRARY(cocaine-plugin-javascript MODULE
    javascript)

TARGET_LINK_LIBRARIES(cocaine-plugin-javascript
    cocaine-core-config
    cocaine-core
    json
    ${BOOST_SYSTEM_LIBS}
    boost_filesystem-mt
    v8
    uriparser)

INSTALL(TARGETS cocaine-plugin-javascript
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
