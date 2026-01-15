/**
 * @file os_fibre.h
 * @brief Cooperative fibre scheduler API
 * 
 * ESP32-C6 Zigbee Bridge OS - Fibre (green thread) scheduler
 * 
 * Implements cooperative multitasking with:
 * - Create/switch/yield/sleep operations
 * - Stack-per-fibre model
 * - Round-robin scheduling with sleep support
 */

#ifndef OS_FIBRE_H
#define OS_FIBRE_H

#include "os_types.h"
#include "os_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fibre states */
typedef enum {
    OS_FIBRE_STATE_READY = 0,    /* Ready to run */
    OS_FIBRE_STATE_RUNNING,       /* Currently executing */
    OS_FIBRE_STATE_SLEEPING,      /* Waiting for timeout */
    OS_FIBRE_STATE_BLOCKED,       /* Waiting for event */
    OS_FIBRE_STATE_DEAD,          /* Terminated */
} os_fibre_state_t;

/* Fibre function signature */
typedef void (*os_fibre_fn_t)(void *arg);

/* Fibre handle (opaque) */
typedef struct os_fibre *os_fibre_handle_t;

/* Fibre info structure for inspection */
typedef struct {
    const char *name;
    os_fibre_state_t state;
    uint32_t stack_size;
    uint32_t stack_used;
    uint32_t run_count;
    os_tick_t wake_tick;
} os_fibre_info_t;

/**
 * @brief Initialize the fibre scheduler
 * @return OS_OK on success
 */
os_err_t os_fibre_init(void);

/**
 * @brief Create a new fibre
 * @param fn Entry function
 * @param arg Argument passed to entry function
 * @param name Fibre name (for debugging)
 * @param stack_size Stack size in bytes (0 = default)
 * @param out_handle Output handle (optional, can be NULL)
 * @return OS_OK on success
 */
os_err_t os_fibre_create(os_fibre_fn_t fn, void *arg, const char *name,
                         uint32_t stack_size, os_fibre_handle_t *out_handle);

/**
 * @brief Start the fibre scheduler (never returns)
 */
void os_fibre_start(void);

/**
 * @brief Yield to the next ready fibre
 */
void os_yield(void);

/**
 * @brief Sleep for specified milliseconds
 * @param ms Milliseconds to sleep
 */
void os_sleep(os_time_ms_t ms);

/**
 * @brief Get current tick count
 * @return Current tick value
 */
os_tick_t os_now_ticks(void);

/**
 * @brief Get uptime in milliseconds
 * @return Uptime in ms
 */
os_time_ms_t os_uptime_ms(void);

/**
 * @brief Get number of active fibres
 * @return Fibre count
 */
uint32_t os_fibre_count(void);

/**
 * @brief Get information about a fibre by index
 * @param index Fibre index (0 to count-1)
 * @param info Output info structure
 * @return OS_OK on success
 */
os_err_t os_fibre_get_info(uint32_t index, os_fibre_info_t *info);

/**
 * @brief Get the current fibre handle
 * @return Handle of currently executing fibre
 */
os_fibre_handle_t os_fibre_current(void);

/**
 * @brief Called by platform timer ISR to advance ticks
 * @note Must be safe to call from ISR context
 */
void os_tick_advance(void);

#ifdef __cplusplus
}
#endif

#endif /* OS_FIBRE_H */
