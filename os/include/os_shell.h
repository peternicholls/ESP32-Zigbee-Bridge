/**
 * @file os_shell.h
 * @brief Interactive shell API
 * 
 * ESP32-C6 Zigbee Bridge OS - Shell commands
 */

#ifndef OS_SHELL_H
#define OS_SHELL_H

#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Command handler signature */
typedef int (*os_shell_cmd_fn_t)(int argc, char *argv[]);

/* Command registration structure */
typedef struct {
    const char *name;
    const char *help;
    os_shell_cmd_fn_t handler;
} os_shell_cmd_t;

/**
 * @brief Initialize shell
 * @return OS_OK on success
 */
os_err_t os_shell_init(void);

/**
 * @brief Register a command
 * @param cmd Command definition
 * @return OS_OK on success
 */
os_err_t os_shell_register(const os_shell_cmd_t *cmd);

/**
 * @brief Shell task entry (to run as fibre)
 * @param arg Unused
 */
void os_shell_task(void *arg);

/**
 * @brief Process a command line
 * @param line Command line to process
 * @return Command return value
 */
int os_shell_process(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* OS_SHELL_H */
