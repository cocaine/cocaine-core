ADD_LIBRARY(yappi-plugin-http MODULE
    http)

TARGET_LINK_LIBRARIES(yappi-plugin-http
    yappi-core-security
    curl)

INSTALL(TARGETS yappi-plugin-http
    LIBRARY DESTINATION lib/yappi
    COMPONENT runtime)
