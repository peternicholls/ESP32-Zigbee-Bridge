/**
 * @file os_console.h
 * @brief UART console driver API
 * 
 * ESP32-C6 Zigbee Bridge OS - Console I/O
 */

#ifndef OS_CONSOLE_H
#define OS_CONSOLE_H

#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize console
 * @return OS_OK on success
 */
os_err_t os_console_init(void);

/**
 * @brief Write a character to console
 * @param c Character to write
 */
void os_console_putc(char c);

/**
 * @brief Write a string to console
 * @param str String to write
 */
void os_console_puts(const char *str);

/**
 * @brief Read a character from console (non-blocking)
 * @return Character read, or -1 if no data available
 */
int os_console_getc(void);

/**
 * @brief Check if data is available to read
 * @return true if data available
 */
bool os_console_available(void);

/**
 * @brief Read a line of input
 * @param buf Output buffer
 * @param max_len Buffer size
 * @param echo Echo characters back
 * @return Length of line read, -1 if not complete
 */
int os_console_readline(char *buf, size_t max_len, bool echo);

#ifdef __cplusplus
}
#endif

#endif /* OS_CONSOLE_H */
