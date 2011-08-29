FIND_PATH(LIBCURL_HEADER_PATH "curl.h" PATH_SUFFIXES "curl" "libcurl")

IF(NOT LIBCURL_HEADER_PATH)
    MESSAGE(FATAL_ERROR "CURL development headers are required.")
ENDIF()

ADD_LIBRARY(yappi-plugin-http MODULE
    http)

TARGET_LINK_LIBRARIES(yappi-plugin-http
    crypto
    curl)

INSTALL(TARGETS yappi-plugin-http
    LIBRARY DESTINATION lib/${PROJECT_NAME}
    COMPONENT runtime)
