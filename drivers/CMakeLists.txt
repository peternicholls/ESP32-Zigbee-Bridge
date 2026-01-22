# Drivers component - Hardware drivers (Zigbee, GPIO, I2C sensors)

# Select Zigbee adapter: real for ESP32-C6 target, fake for host simulation
if(IDF_TARGET STREQUAL "esp32c6")
    set(ZB_ADAPTER_SRC "zigbee/zb_real.c" "zigbee/zb_cmd.c" "zigbee/zb_callback.c")
    set(ZB_REQUIRES os driver espressif__esp-zigbee-lib)
else()
    set(ZB_ADAPTER_SRC "zigbee/zb_fake.c")
    set(ZB_REQUIRES os driver)
endif()

idf_component_register(
    SRCS
        ${ZB_ADAPTER_SRC}
        "gpio_button/gpio_button.c"
        "i2c_sensor/i2c_sensor.c"
    INCLUDE_DIRS
        "zigbee"
        "gpio_button"
        "i2c_sensor"
    REQUIRES
        ${ZB_REQUIRES}
)
