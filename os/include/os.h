/**
 * @file os.h
 * @brief Main OS header - includes all OS components
 * 
 * ESP32-C6 Zigbee Bridge OS
 */

#ifndef OS_H
#define OS_H

#include "os_types.h"
#include "os_config.h"
#include "os_fibre.h"
#include "os_event.h"
#include "os_log.h"
#include "os_console.h"
#include "os_shell.h"
#include "os_persist.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all OS components
 * @return OS_OK on success
 */
os_err_t os_init(void);

/**
 * @brief Start the OS (never returns)
 */
void os_start(void);

#ifdef __cplusplus
}
#endif

#endif /* OS_H */
