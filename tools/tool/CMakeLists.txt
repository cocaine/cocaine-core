ADD_EXECUTABLE(cocaine-tool
    tool)

TARGET_LINK_LIBRARIES(cocaine-tool
    boost_program_options-mt
    cocaine-core)

INSTALL(
    TARGETS
        cocaine-tool
    RUNTIME DESTINATION bin
    COMPONENT runtime)
