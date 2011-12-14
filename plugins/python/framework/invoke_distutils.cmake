EXECUTE_PROCESS(
    COMMAND python setup.py install --prefix=${CMAKE_INSTALL_PREFIX} --install-layout=deb
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/python/framework
)
