/**
 * @file i2c_sensor.h
 * @brief I2C sensor driver (host simulation)
 */

#ifndef I2C_SENSOR_H
#define I2C_SENSOR_H

#include "os_types.h"

os_err_t i2c_sensor_init(void);
float i2c_sensor_read_temperature_c(void);

#endif /* I2C_SENSOR_H */
