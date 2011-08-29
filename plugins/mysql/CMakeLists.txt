FIND_PATH(MYSQL_HEADER_PATH "mysql.h" PATH_SUFFIXES "mysql" "libmysql")

FIND_LIBRARY(LIBMYSQL "mysqlclient" PATH_SUFFIXES "mysqlclient" "libmysqlclient")
GET_FILENAME_COMPONENT(LIBMYSQL_PATH ${LIBMYSQL} PATH)

IF(NOT MYSQL_HEADER_PATH OR NOT LIBMYSQL_PATH)
    MESSAGE(FATAL_ERROR "MySQL development files are required.")
ENDIF()

INCLUDE_DIRECTORIES(${MYSQL_HEADER_PATH})
LINK_DIRECTORIES(${LIBMYSQL_PATH})

ADD_LIBRARY(yappi-plugin-mysql MODULE
    mysql.cpp)

TARGET_LINK_LIBRARIES(yappi-plugin-mysql
    crypto
    mysqlclient
    uriparser)

INSTALL(TARGETS yappi-plugin-mysql
    LIBRARY DESTINATION lib/${PROJECT_NAME}
    COMPONENT runtime)
