/**
 * @file app_blink.c
 * @brief Blink demo task implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - Demo task
 * 
 * Demonstrates sleep/yield usage in a fibre.
 */

#include "app_blink.h"
#include "os.h"

#define BLINK_MODULE "BLINK"
#define BLINK_INTERVAL_MS  500

static volatile uint32_t blink_count = 0;

void app_blink_task(void *arg) {
    (void)arg;
    
    LOG_I(BLINK_MODULE, "Blink task started");
    
    while (1) {
        blink_count++;
        LOG_D(BLINK_MODULE, "Blink %u (on)", blink_count);
        
        /* On a real ESP32, this would toggle a GPIO */
        /* For now, just demonstrate sleep */
        os_sleep(BLINK_INTERVAL_MS);
        
        LOG_T(BLINK_MODULE, "Blink %u (off)", blink_count);
        os_sleep(BLINK_INTERVAL_MS);
    }
}

uint32_t app_blink_count(void) {
    return blink_count;
}
