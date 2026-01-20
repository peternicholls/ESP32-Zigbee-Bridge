# Apps component - Demo applications (blink, etc.)

idf_component_register(
    SRCS
        "src/app_blink.c"
    INCLUDE_DIRS
        "src"
    REQUIRES
        os
    PRIV_REQUIRES
        espressif__led_strip
)
