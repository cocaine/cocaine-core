EXECUTE_PROCESS(
    COMMAND python -c "from distutils import sysconfig; print sysconfig.get_python_lib()"
    OUTPUT_VARIABLE PYTHON_LIBRARY_DIRS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

IF(NOT PYTHON_LIBRARY_DIRS)
    MESSAGE(FATAL_ERROR "No python distribution found.")
ELSE()
    INSTALL(DIRECTORY cocaine
        DESTINATION ${PYTHON_LIBRARY_DIRS}
        COMPONENT runtime)
ENDIF()
