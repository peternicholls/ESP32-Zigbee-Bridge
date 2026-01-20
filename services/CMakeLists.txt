# Services component - Domain services (registry, interview, capability, HA discovery)

idf_component_register(
    SRCS
        "src/registry.c"
        "src/reg_shell.c"
        "src/interview.c"
        "src/capability.c"
        "src/quirks.c"
        "ha_disc/ha_disc.c"
        "local_node/local_node.c"
    INCLUDE_DIRS
        "include"
        "ha_disc"
        "local_node"
    REQUIRES
        os
        drivers
    PRIV_REQUIRES
        adapters
)
