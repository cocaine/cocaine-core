FIND_PACKAGE(CURL)

IF(NOT CURL_FOUND)
    MESSAGE(WARNING "libcurl development files are required to build this plugin.")
    RETURN()
ENDIF()

ADD_LIBRARY(yappi-plugin-http MODULE
    http)

TARGET_LINK_LIBRARIES(yappi-plugin-http
    crypto
    curl)

INSTALL(TARGETS yappi-plugin-http
    LIBRARY DESTINATION lib/yappi
    COMPONENT runtime)
