/**
 * @file gpio_button.h
 * @brief GPIO button driver (host simulation)
 */

#ifndef GPIO_BUTTON_H
#define GPIO_BUTTON_H

#include "os_types.h"

os_err_t gpio_button_init(void);
bool gpio_button_read(void);

#endif /* GPIO_BUTTON_H */
