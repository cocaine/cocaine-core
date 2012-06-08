ADD_EXECUTABLE(cocaine-upload
    upload)

TARGET_LINK_LIBRARIES(cocaine-upload
    boost_program_options-mt
    cocaine-core)

INSTALL(
    TARGETS
        cocaine-upload
    RUNTIME DESTINATION bin COMPONENT runtime)
