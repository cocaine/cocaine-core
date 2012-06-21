ADD_EXECUTABLE(cocaine-deploy
    deploy)

TARGET_LINK_LIBRARIES(cocaine-deploy
    boost_program_options-mt
    cocaine-core)

INSTALL(
    TARGETS
        cocaine-deploy
    RUNTIME DESTINATION bin
    COMPONENT runtime)
