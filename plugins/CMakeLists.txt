SET(PLUGINS
    mysql
    http
    python
    inject
    javascript)

FOREACH(PLUGIN ${PLUGINS})
    ADD_SUBDIRECTORY(${PLUGIN})
ENDFOREACH(PLUGIN)
