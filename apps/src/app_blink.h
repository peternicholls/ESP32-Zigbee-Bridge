/**
 * @file app_blink.h
 * @brief Blink demo task
 * 
 * ESP32-C6 Zigbee Bridge OS - Demo task that uses sleep/yield
 */

#ifndef APP_BLINK_H
#define APP_BLINK_H

#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Blink task entry function
 * @param arg Unused
 */
void app_blink_task(void *arg);

/**
 * @brief Get blink count
 * @return Number of blink cycles
 */
uint32_t app_blink_count(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_BLINK_H */
