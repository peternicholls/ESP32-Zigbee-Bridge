/**
 * @file i2c_sensor.c
 * @brief I2C sensor driver (host simulation)
 */

#include "i2c_sensor.h"
#include "os_fibre.h"

static bool initialized = false;

os_err_t i2c_sensor_init(void) {
    initialized = true;
    return OS_OK;
}

float i2c_sensor_read_temperature_c(void) {
    if (!initialized) {
        return 0.0f;
    }

    os_tick_t ticks = os_now_ticks();
    float phase = (float)(ticks % OS_MS_TO_TICKS(10000)) / (float)OS_MS_TO_TICKS(10000);
    return 20.0f + (5.0f * phase);
}
