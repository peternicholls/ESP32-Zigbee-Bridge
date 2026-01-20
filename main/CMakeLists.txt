# Main component - Application entry point

idf_component_register(
    SRCS
        "src/main.c"
    INCLUDE_DIRS
        "src"
    REQUIRES
        os
        services
        adapters
        drivers
        apps
)
