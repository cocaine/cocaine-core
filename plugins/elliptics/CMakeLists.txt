IF(ELLIPTICS)
    LOCATE_LIBRARY(LIBELLIPTICSCLIENT "cppdef.h" "elliptics_cpp" "elliptics")

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
