# Drivers component - Hardware drivers (Zigbee, GPIO, I2C sensors)

# Conditional Zigbee adapter: real implementation for ESP32-C6, fake for host.
# CONFIG_IDF_TARGET_ESP32C6 is only defined when building with ESP-IDF (idf.py).
# Host builds (make all/test) use the else branch with zb_fake.c.
if(CONFIG_IDF_TARGET_ESP32C6)
    set(ZIGBEE_SRCS
        "zigbee/zb_real.c"
        "zigbee/zb_cmd.c"
        "zigbee/zb_shell.c"
    )
    idf_component_register(
        SRCS
            ${ZIGBEE_SRCS}
            "gpio_button/gpio_button.c"
            "i2c_sensor/i2c_sensor.c"
        INCLUDE_DIRS
            "zigbee"
            "gpio_button"
            "i2c_sensor"
        REQUIRES
            os
            driver
        PRIV_REQUIRES
            espressif__esp-zigbee-lib
            espressif__esp-zboss-lib
            ieee802154
    )
    # Explicitly add managed component and IDF component include directories
    # (workaround for PRIV_REQUIRES not propagating includes correctly)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${PROJECT_DIR}/managed_components/espressif__esp-zigbee-lib/include"
        "${PROJECT_DIR}/managed_components/espressif__esp-zboss-lib/include"
        "${IDF_PATH}/components/ieee802154/include"
    )
else()
    set(ZIGBEE_SRCS
        "zigbee/zb_fake.c"
    )
    idf_component_register(
        SRCS
            ${ZIGBEE_SRCS}
            "gpio_button/gpio_button.c"
            "i2c_sensor/i2c_sensor.c"
        INCLUDE_DIRS
            "zigbee"
            "gpio_button"
            "i2c_sensor"
        REQUIRES
            os
            driver
    )
endif()
