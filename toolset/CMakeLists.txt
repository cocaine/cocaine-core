EXECUTE_PROCESS(
    COMMAND python -c "from distutils import sysconfig; print sysconfig.get_python_lib()"
    OUTPUT_VARIABLE PYTHON_LIBRARY_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

IF(NOT PYTHON_LIBRARY_PATH)
    MESSAGE("No python distribution found on your system")
ELSE()
    INSTALL(DIRECTORY yappi
        DESTINATION ${PYTHON_LIBRARY_PATH}
        COMPONENT runtime)
ENDIF()
