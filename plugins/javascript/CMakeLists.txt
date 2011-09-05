LOCATE_LIBRARY(LIBV8 "v8.h" "v8")

ADD_LIBRARY(yappi-plugin-javascript MODULE
    javascript)

TARGET_LINK_LIBRARIES(yappi-plugin-javascript
    v8
    uriparser)

INSTALL(TARGETS yappi-plugin-javascript
    LIBRARY DESTINATION lib/yappi
    COMPONENT runtime)
