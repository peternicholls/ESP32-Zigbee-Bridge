/**
 * @file gpio_button.c
 * @brief GPIO button driver (host simulation)
 */

#include "gpio_button.h"
#include "os_fibre.h"

/* Period in milliseconds for the simulated button toggle behavior. */
#define GPIO_BUTTON_TOGGLE_MS 1500

static bool initialized = false;

os_err_t gpio_button_init(void) {
    initialized = true;
    return OS_OK;
}

bool gpio_button_read(void) {
    if (!initialized) {
        return false;
    }

    os_tick_t ticks = os_now_ticks();
    return ((ticks / OS_MS_TO_TICKS(GPIO_BUTTON_TOGGLE_MS)) % 2) == 1;
}
