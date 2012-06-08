ADD_LIBRARY(native-server MODULE
	native_job
    native_server)

TARGET_LINK_LIBRARIES(native-server
    cocaine-core)

SET_TARGET_PROPERTIES(python PROPERTIES
    PREFIX ""
    SUFFIX ".cocaine-plugin")

INSTALL(TARGETS native-server
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
