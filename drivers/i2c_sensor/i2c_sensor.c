/**
 * @file i2c_sensor.c
 * @brief I2C sensor driver (host simulation)
 */

#include "i2c_sensor.h"
#include "os_fibre.h"

/* Simulation parameters for temperature cycling */
#define TEMP_CYCLE_MS      10000   /* Period of simulated temperature variation in milliseconds */
#define TEMP_BASE_C        20.0f   /* Base temperature in Celsius */
#define TEMP_VARIATION_C   5.0f    /* Temperature variation range in Celsius */

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
    float phase = (float)(ticks % OS_MS_TO_TICKS(TEMP_CYCLE_MS)) / (float)OS_MS_TO_TICKS(TEMP_CYCLE_MS);
    return TEMP_BASE_C + (TEMP_VARIATION_C * phase);
}
