ADD_LIBRARY(chrono MODULE
    recurring_timer
    drifting_timer
    module)

TARGET_LINK_LIBRARIES(chrono
    cocaine-core)

SET_TARGET_PROPERTIES(chrono PROPERTIES
    PREFIX ""
    SUFFIX ".cocaine-plugin")

INSTALL(TARGETS chrono
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
