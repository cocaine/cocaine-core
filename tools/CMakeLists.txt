ADD_EXECUTABLE(cocaine-tool
    tool)

TARGET_LINK_LIBRARIES(cocaine-tool
    boost_program_options-mt
    cocaine-core)

SET_TARGET_PROPERTIES(cocaine-tool PROPERTIES
    COMPILE_FLAGS "-std=c++0x")

INSTALL(
    TARGETS
        cocaine-tool
    RUNTIME DESTINATION bin
    COMPONENT runtime)
