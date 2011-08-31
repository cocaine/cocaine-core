ADD_LIBRARY(yappi-plugin-javascript MODULE
    javascript)

TARGET_LINK_LIBRARIES(yappi-plugin-javascript
    yappi-core-security
    v8
    uriparser)

INSTALL(TARGETS yappi-plugin-javascript
    LIBRARY DESTINATION lib/yappi
    COMPONENT runtime)
