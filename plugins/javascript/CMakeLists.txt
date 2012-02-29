LOCATE_LIBRARY(LIBV8 "v8.h" "v8")

ADD_LIBRARY(cocaine-plugin-javascript MODULE
    javascript)

TARGET_LINK_LIBRARIES(cocaine-plugin-javascript
    cocaine-core
    v8)

INSTALL(TARGETS cocaine-plugin-javascript
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
