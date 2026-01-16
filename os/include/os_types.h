/**
 * @file os_types.h
 * @brief Common types for the Tiny OS
 * 
 * ESP32-C6 Zigbee Bridge OS - Common type definitions
 */

#ifndef OS_TYPES_H
#define OS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Error codes */
typedef enum {
    OS_OK = 0,
    OS_ERR_INVALID_ARG = -1,
    OS_ERR_NO_MEM = -2,
    OS_ERR_TIMEOUT = -3,
    OS_ERR_FULL = -4,
    OS_ERR_EMPTY = -5,
    OS_ERR_NOT_FOUND = -6,
    OS_ERR_BUSY = -7,
    OS_ERR_ALREADY_EXISTS = -8,
    OS_ERR_NOT_INITIALIZED = -9,
    OS_ERR_NOT_READY = -10,
} os_err_t;

/* Time types */
typedef uint32_t os_tick_t;
typedef uint32_t os_time_ms_t;

/* Ticks per millisecond (configurable) */
#define OS_TICKS_PER_MS  1

/* Convert ms to ticks */
#define OS_MS_TO_TICKS(ms)  ((os_tick_t)((ms) * OS_TICKS_PER_MS))

/* Convert ticks to ms */
#define OS_TICKS_TO_MS(ticks)  ((os_time_ms_t)((ticks) / OS_TICKS_PER_MS))

/* Maximum values */
#define OS_WAIT_FOREVER  UINT32_MAX

/* String size limits */
#define OS_NAME_MAX_LEN  16
#define OS_LOG_MSG_MAX_LEN  128

/* Correlation ID type */
typedef uint32_t os_corr_id_t;

/* EUI64 type for Zigbee addresses */
typedef uint64_t os_eui64_t;

/* Helper macros for EUI64 */
#define OS_EUI64_FMT  "%016llX"
#define OS_EUI64_ARG(eui)  ((unsigned long long)(eui))

#endif /* OS_TYPES_H */
