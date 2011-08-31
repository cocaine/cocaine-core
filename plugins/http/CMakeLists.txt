ADD_LIBRARY(yappi-plugin-http MODULE
    http)

TARGET_LINK_LIBRARIES(yappi-plugin-http
    yappi-core-security
    curl)

ADD_DEPENDENCIES(yappi-plugin-http
    yappi-core-security)

INSTALL(TARGETS yappi-plugin-http
    LIBRARY DESTINATION lib/yappi
    COMPONENT runtime)
