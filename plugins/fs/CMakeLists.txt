ADD_LIBRARY(fs MODULE
	filesystem_monitor)

TARGET_LINK_LIBRARIES(fs
    cocaine-core)

SET_TARGET_PROPERTIES(fs PROPERTIES
    PREFIX ""
    SUFFIX ".cocaine-plugin")

INSTALL(TARGETS fs
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
