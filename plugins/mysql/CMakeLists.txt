FIND_FILE(MYSQL_HEADER "mysql.h" PATH_SUFFIXES "mysql" "libmysql")
GET_FILENAME_COMPONENT(MYSQL_HEADER_PATH ${MYSQL_HEADER} PATH)

FIND_LIBRARY(LIBMYSQL "mysqlclient" PATH_SUFFIXES "mysqlclient" "libmysqlclient")
GET_FILENAME_COMPONENT(LIBMYSQL_PATH ${LIBMYSQL} PATH)

IF(NOT MYSQL_HEADER OR NOT LIBMYSQL)
    MESSAGE("MySQL development files are required to build mysql plugin.")
    RETURN()
ENDIF()

INCLUDE_DIRECTORIES(${MYSQL_HEADER_PATH})

ADD_LIBRARY(yappi-plugin-mysql MODULE
    mysql.cpp)

SET_TARGET_PROPERTIES(yappi-plugin-mysql PROPERTIES
    LINK_FLAGS "-L${LIBMYSQL_PATH}")

TARGET_LINK_LIBRARIES(yappi-plugin-mysql
    crypto
    mysqlclient
    uriparser)
