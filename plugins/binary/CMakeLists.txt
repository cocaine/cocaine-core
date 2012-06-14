ADD_LIBRARY(binary MODULE binary.cpp)

TARGET_LINK_LIBRARIES(binary cocaine-common)

SET_TARGET_PROPERTIES(binary PROPERTIES
PREFIX ""
SUFFIX ".cocaine-plugin")

INSTALL(TARGETS binary
	LIBRARY DESTINATION lib/cocaine
	COMPONENT runtime)
