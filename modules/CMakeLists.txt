SET(MODULES
        python
        javascript
        mongo
        test)

IF(NOT APPLE)
    SET(MODULES ${MODULES} perl)
ENDIF()

FOREACH(MODULE ${MODULES})
    ADD_SUBDIRECTORY(${MODULE})
ENDFOREACH(MODULE)
