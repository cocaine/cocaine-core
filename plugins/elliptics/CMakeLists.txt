IF(ELLIPTICS)
    LOCATE_LIBRARY(LIBELLIPTICSCLIENT "cppdef.h" "elliptics_cpp" "elliptics")
    LOCATE_LIBRARY(LIBEBLOBCLIENT "eblob.hpp" "eblob_cpp" "eblob")

    INCLUDE_DIRECTORIES(${LIBELLIPTICSCLIENT_INCLUDE_DIRS} ${LIBEBLOBCLIENT_INCLUDE_DIRS})
    LINK_DIRECTORIES(${LIBELLIPTICSCLIENT_LIBRARY_DIRS} ${LIBEBLOBCLIENT_LIBRARY_DIRS})
    
    ADD_LIBRARY(elliptics MODULE
        elliptics)

    TARGET_LINK_LIBRARIES(elliptics
        cocaine-common
        elliptics_cpp)

    SET_TARGET_PROPERTIES(elliptics PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin")

    INSTALL(TARGETS elliptics
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(ELLIPTICS)
