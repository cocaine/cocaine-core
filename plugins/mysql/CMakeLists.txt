LOCATE_LIBRARY(LIBMYSQL "mysql.h" "mysqlclient" "mysql")

INCLUDE_DIRECTORIES(${LIBMYSQL_INCLUDE_DIRS})
LINK_DIRECTORIES(${LIBMYSQL_LIBRARY_DIRS})

ADD_LIBRARY(cocaine-plugin-mysql MODULE
    mysql.cpp)

TARGET_LINK_LIBRARIES(cocaine-plugin-mysql
    json
    mysqlclient
    uriparser)

INSTALL(TARGETS cocaine-plugin-mysql
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
