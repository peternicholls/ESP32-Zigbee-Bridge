# Drivers component - Hardware drivers (Zigbee, GPIO, I2C sensors)

idf_component_register(
    SRCS
        "zigbee/zb_fake.c"
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
