/**
 * @file os_log.c
 * @brief Structured logging implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - Logging system
 * 
 * Uses a ring buffer queue for ISR-safe logging.
 */

#include "os_log.h"
#include "os_fibre.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdarg.h>

/* Log entry structure */
typedef struct {
    os_log_level_t level;
    os_tick_t timestamp;
    char module[8];
    char message[OS_LOG_MSG_MAX_LEN];
} log_entry_t;

/* Log queue state */
static struct {
    bool initialized;
    os_log_level_t level;
    
    /* Ring buffer */
    log_entry_t queue[OS_LOG_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
    
    /* Statistics */
    uint32_t dropped;
} logger = {0};

/* Level names */
static const char *level_names[] = {
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "TRACE"
};

os_err_t os_log_init(void) {
    if (logger.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
    memset(&logger, 0, sizeof(logger));
    logger.level = OS_LOG_DEFAULT_LEVEL;
    logger.initialized = true;
    
    return OS_OK;
}

void os_log_set_level(os_log_level_t level) {
    if (level < OS_LOG_LEVEL_COUNT) {
        logger.level = level;
    }
}

os_log_level_t os_log_get_level(void) {
    return logger.level;
}

void os_log_write(os_log_level_t level, const char *module, const char *fmt, ...) {
    if (!logger.initialized) {
        return;
    }
    
    /* Filter by level */
    if (level > logger.level) {
        return;
    }
    
    /* Check for queue space */
    if (logger.count >= OS_LOG_QUEUE_SIZE) {
        logger.dropped++;
        return;
    }
    
    /* Get slot */
    log_entry_t *entry = &logger.queue[logger.tail];
    
    entry->level = level;
    entry->timestamp = os_now_ticks();
    
    /* Copy module name */
    strncpy(entry->module, module ? module : "???", sizeof(entry->module) - 1);
    entry->module[sizeof(entry->module) - 1] = '\0';
    
    /* Format message */
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->message, sizeof(entry->message), fmt, args);
    va_end(args);
    
    /* Advance tail */
    logger.tail = (logger.tail + 1) % OS_LOG_QUEUE_SIZE;
    logger.count++;
}

uint32_t os_log_flush(void) {
    if (!logger.initialized || logger.count == 0) {
        return 0;
    }
    
    uint32_t flushed = 0;
    
    while (logger.count > 0) {
        log_entry_t *entry = &logger.queue[logger.head];
        
        /* Output format: [T][LEVEL][MODULE] message */
        printf("[%08u][%-5s][%-6s] %s\n",
               (unsigned)entry->timestamp,
               level_names[entry->level],
               entry->module,
               entry->message);
        
        /* Advance head */
        logger.head = (logger.head + 1) % OS_LOG_QUEUE_SIZE;
        logger.count--;
        flushed++;
    }
    
    return flushed;
}

const char *os_log_level_name(os_log_level_t level) {
    if (level < OS_LOG_LEVEL_COUNT) {
        return level_names[level];
    }
    return "?????";
}

os_log_level_t os_log_level_parse(const char *name) {
    if (name == NULL) {
        return OS_LOG_LEVEL_INFO;
    }
    
    for (uint32_t i = 0; i < OS_LOG_LEVEL_COUNT; i++) {
        if (strcasecmp(name, level_names[i]) == 0) {
            return (os_log_level_t)i;
        }
    }
    
    return OS_LOG_LEVEL_INFO;
}
