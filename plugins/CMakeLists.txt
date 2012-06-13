SET(PLUGINS
        python
        javascript
        mongo
        elliptics
        chrono
        fs
        native-server)

IF(NOT APPLE)
    SET(PLUGINS ${PLUGINS} perl)
ENDIF()

FOREACH(PLUGIN ${PLUGINS})
    ADD_SUBDIRECTORY(${PLUGIN})
ENDFOREACH(PLUGIN)
