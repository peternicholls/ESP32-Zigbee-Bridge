/**
 * @file os_log.h
 * @brief Structured logging API
 * 
 * ESP32-C6 Zigbee Bridge OS - Logging system
 * 
 * Features:
 * - Multiple log levels (ERROR, WARN, INFO, DEBUG, TRACE)
 * - Module-tagged messages
 * - Safe from ISR/callback context (enqueue based)
 * - Structured output format
 */

#ifndef OS_LOG_H
#define OS_LOG_H

#include "os_types.h"
#include "os_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Log levels */
typedef enum {
    OS_LOG_LEVEL_ERROR = 0,
    OS_LOG_LEVEL_WARN,
    OS_LOG_LEVEL_INFO,
    OS_LOG_LEVEL_DEBUG,
    OS_LOG_LEVEL_TRACE,
    OS_LOG_LEVEL_COUNT
} os_log_level_t;

/**
 * @brief Initialize the logging system
 * @return OS_OK on success
 */
os_err_t os_log_init(void);

/**
 * @brief Set the current log level
 * @param level Minimum level to output
 */
void os_log_set_level(os_log_level_t level);

/**
 * @brief Get the current log level
 * @return Current log level
 */
os_log_level_t os_log_get_level(void);

/**
 * @brief Write a log message
 * @param level Log level
 * @param module Module name (short, e.g., "OS", "ZB", "MQTT")
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void os_log_write(os_log_level_t level, const char *module, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * @brief Flush log queue to output
 * @return Number of messages flushed
 */
uint32_t os_log_flush(void);

/**
 * @brief Get log level name string
 * @param level Log level
 * @return Level name string
 */
const char *os_log_level_name(os_log_level_t level);

/**
 * @brief Parse log level from string
 * @param name Level name string
 * @return Log level, or OS_LOG_LEVEL_INFO on error
 */
os_log_level_t os_log_level_parse(const char *name);

/* Convenience macros */
#define LOG_E(module, fmt, ...) os_log_write(OS_LOG_LEVEL_ERROR, module, fmt, ##__VA_ARGS__)
#define LOG_W(module, fmt, ...) os_log_write(OS_LOG_LEVEL_WARN, module, fmt, ##__VA_ARGS__)
#define LOG_I(module, fmt, ...) os_log_write(OS_LOG_LEVEL_INFO, module, fmt, ##__VA_ARGS__)
#define LOG_D(module, fmt, ...) os_log_write(OS_LOG_LEVEL_DEBUG, module, fmt, ##__VA_ARGS__)
#define LOG_T(module, fmt, ...) os_log_write(OS_LOG_LEVEL_TRACE, module, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* OS_LOG_H */
