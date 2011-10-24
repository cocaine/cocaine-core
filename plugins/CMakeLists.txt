SET(PLUGINS
    mysql
    http
    python
    javascript
    perl)

FOREACH(PLUGIN ${PLUGINS})
    ADD_SUBDIRECTORY(${PLUGIN})
ENDFOREACH(PLUGIN)
