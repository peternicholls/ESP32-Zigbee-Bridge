# Adapters component - Northbound protocol adapters (MQTT, etc.)

idf_component_register(
    SRCS
        "mqtt_adapter/mqtt_adapter.c"
    INCLUDE_DIRS
        "mqtt_adapter"
    REQUIRES
        os
        services
        esp_wifi
)
