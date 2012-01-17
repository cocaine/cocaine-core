SET(PLUGINS
        mysql
        http
        python
        javascript)

IF(NOT APPLE)
    SET(PLUGINS ${PLUGINS} perl)
ENDIF()

FOREACH(PLUGIN ${PLUGINS})
    ADD_SUBDIRECTORY(${PLUGIN})
ENDFOREACH(PLUGIN)
