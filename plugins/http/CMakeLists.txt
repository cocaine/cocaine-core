ADD_LIBRARY(cocaine-plugin-http MODULE
    http)

TARGET_LINK_LIBRARIES(cocaine-plugin-http
    curl)

INSTALL(TARGETS cocaine-plugin-http
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
