IF (JAVASCRIPT)
	LOCATE_LIBRARY(LIBV8 "v8.h" "v8")

	ADD_LIBRARY(javascript MODULE
	    javascript)

	TARGET_LINK_LIBRARIES(javascript
	    cocaine-core
	    v8)

	SET_TARGET_PROPERTIES(javascript PROPERTIES
	    PREFIX ""
	    SUFFIX ".cocaine-module")

	INSTALL(TARGETS javascript
	    LIBRARY DESTINATION lib/cocaine
	    COMPONENT runtime)
ENDIF()
