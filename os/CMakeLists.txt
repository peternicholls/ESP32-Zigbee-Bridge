# OS component - Tiny OS core (fibres, events, logging, shell, persistence)

idf_component_register(
    SRCS
        "src/os.c"
        "src/os_fibre.c"
        "src/os_event.c"
        "src/os_log.c"
        "src/os_console.c"
        "src/os_shell.c"
        "src/os_persist.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        nvs_flash
        esp_timer
    PRIV_REQUIRES
        adapters
)
