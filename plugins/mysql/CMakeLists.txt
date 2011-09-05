LOCATE_LIBRARY(LIBMYSQL "mysql.h" "mysqlclient" "mysql")

INCLUDE_DIRECTORIES(${LIBMYSQL_INCLUDE_DIRS})
LINK_DIRECTORIES(${LIBMYSQL_LIBRARY_DIRS})

ADD_LIBRARY(yappi-plugin-mysql MODULE
    mysql.cpp)

TARGET_LINK_LIBRARIES(yappi-plugin-mysql
    mysqlclient
    uriparser)

INSTALL(TARGETS yappi-plugin-mysql
    LIBRARY DESTINATION lib/yappi
    COMPONENT runtime)
