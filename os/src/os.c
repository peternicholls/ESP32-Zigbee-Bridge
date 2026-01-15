/**
 * @file os.c
 * @brief Main OS initialization
 * 
 * ESP32-C6 Zigbee Bridge OS
 */

#include "os.h"

#define OS_MODULE "OS"

os_err_t os_init(void) {
    os_err_t err;
    
    /* Initialize console first (for early output) */
    err = os_console_init();
    if (err != OS_OK && err != OS_ERR_ALREADY_EXISTS) {
        return err;
    }
    
    /* Initialize logging */
    err = os_log_init();
    if (err != OS_OK && err != OS_ERR_ALREADY_EXISTS) {
        return err;
    }
    
    LOG_I(OS_MODULE, "Initializing OS...");
    
    /* Initialize event bus */
    err = os_event_init();
    if (err != OS_OK && err != OS_ERR_ALREADY_EXISTS) {
        LOG_E(OS_MODULE, "Event bus init failed: %d", err);
        return err;
    }
    LOG_D(OS_MODULE, "Event bus initialized");
    
    /* Initialize fibre scheduler */
    err = os_fibre_init();
    if (err != OS_OK && err != OS_ERR_ALREADY_EXISTS) {
        LOG_E(OS_MODULE, "Fibre scheduler init failed: %d", err);
        return err;
    }
    LOG_D(OS_MODULE, "Fibre scheduler initialized");
    
    /* Initialize shell */
    err = os_shell_init();
    if (err != OS_OK && err != OS_ERR_ALREADY_EXISTS) {
        LOG_E(OS_MODULE, "Shell init failed: %d", err);
        return err;
    }
    LOG_D(OS_MODULE, "Shell initialized");
    
    /* Initialize persistence */
    err = os_persist_init();
    if (err != OS_OK && err != OS_ERR_ALREADY_EXISTS) {
        LOG_E(OS_MODULE, "Persistence init failed: %d", err);
        return err;
    }
    LOG_D(OS_MODULE, "Persistence initialized");
    
    LOG_I(OS_MODULE, "OS initialization complete");
    
    /* Emit boot event */
    os_event_emit(OS_EVENT_BOOT, NULL, 0);
    
    return OS_OK;
}

void os_start(void) {
    LOG_I(OS_MODULE, "Starting fibre scheduler...");
    os_log_flush();
    os_fibre_start();
    /* Never returns */
}
