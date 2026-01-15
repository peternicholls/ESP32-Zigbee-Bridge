/**
 * @file os_config.h
 * @brief Configuration for the Tiny OS
 * 
 * ESP32-C6 Zigbee Bridge OS - Build-time configuration
 */

#ifndef OS_CONFIG_H
#define OS_CONFIG_H

/* Scheduler configuration */
#define OS_MAX_FIBRES           16
#define OS_DEFAULT_STACK_SIZE   2048
#define OS_IDLE_STACK_SIZE      512

/* Event bus configuration */
#define OS_EVENT_QUEUE_SIZE     256
#define OS_MAX_SUBSCRIBERS      32

/* Logging configuration */
#define OS_LOG_QUEUE_SIZE       64
#define OS_LOG_DEFAULT_LEVEL    OS_LOG_LEVEL_INFO

/* Shell configuration */
#define OS_SHELL_LINE_MAX       128
#define OS_SHELL_HISTORY_SIZE   4
#define OS_SHELL_MAX_ARGS       8

/* Persistence configuration */
#define OS_PERSIST_NAMESPACE    "bridge"
#define OS_PERSIST_FLUSH_MS     5000

/* Timer configuration */
#define OS_TIMER_TICK_MS        1

/* Platform detection */
#if defined(ESP_PLATFORM)
    #define OS_PLATFORM_ESP32   1
#else
    #define OS_PLATFORM_HOST    1
#endif

#endif /* OS_CONFIG_H */
