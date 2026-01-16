/**
 * @file os_shell.c
 * @brief Interactive shell implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - Shell with built-in commands
 */

#include "os_shell.h"
#include "os_fibre.h"
#include "os_log.h"
#include "os_console.h"
#include "os_event.h"
#include "os_persist.h"
#include "os_config.h"
#include "mqtt_adapter.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#define SHELL_MODULE "SHELL"
#define MAX_COMMANDS 32

/* Command registry */
static struct {
    bool initialized;
    os_shell_cmd_t commands[MAX_COMMANDS];
    uint32_t count;
} shell = {0};

/* Forward declarations for built-in commands */
static int cmd_help(int argc, char *argv[]);
static int cmd_ps(int argc, char *argv[]);
static int cmd_uptime(int argc, char *argv[]);
static int cmd_loglevel(int argc, char *argv[]);
static int cmd_stats(int argc, char *argv[]);
static int cmd_sched(int argc, char *argv[]);
static int cmd_events(int argc, char *argv[]);
static int cmd_persist(int argc, char *argv[]);
static int cmd_mqtt(int argc, char *argv[]);

/* Built-in commands */
static const os_shell_cmd_t builtin_commands[] = {
    {"help",     "Show available commands",          cmd_help},
    {"ps",       "Show running tasks",               cmd_ps},
    {"uptime",   "Show system uptime",               cmd_uptime},
    {"loglevel", "Get/set log level [level]",        cmd_loglevel},
    {"stats",    "Show event bus statistics",        cmd_stats},
    {"sched",    "Show scheduler statistics",        cmd_sched},
    {"events",   "Alias for 'stats'",                cmd_events},
    {"persist",  "Show persistence statistics",      cmd_persist},
    {"mqtt",     "Show MQTT statistics",             cmd_mqtt},
    {NULL, NULL, NULL}
};

os_err_t os_shell_init(void) {
    if (shell.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
    memset(&shell, 0, sizeof(shell));
    
    /* Register built-in commands */
    for (const os_shell_cmd_t *cmd = builtin_commands; cmd->name != NULL; cmd++) {
        os_shell_register(cmd);
    }
    
    shell.initialized = true;
    return OS_OK;
}

os_err_t os_shell_register(const os_shell_cmd_t *cmd) {
    if (cmd == NULL || cmd->name == NULL || cmd->handler == NULL) {
        return OS_ERR_INVALID_ARG;
    }
    
    if (shell.count >= MAX_COMMANDS) {
        return OS_ERR_FULL;
    }
    
    shell.commands[shell.count++] = *cmd;
    return OS_OK;
}

/* Parse command line into argc/argv */
static int parse_args(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *p = line;
    
    while (*p && argc < max_args) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        
        /* Start of argument */
        argv[argc++] = p;
        
        /* Find end of argument */
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    
    return argc;
}

int os_shell_process(const char *line) {
    if (line == NULL || *line == '\0') {
        return 0;
    }
    
    /* Make mutable copy */
    char buf[OS_SHELL_LINE_MAX];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    
    /* Parse arguments */
    char *argv[OS_SHELL_MAX_ARGS];
    int argc = parse_args(buf, argv, OS_SHELL_MAX_ARGS);
    
    if (argc == 0) {
        return 0;
    }
    
    /* Find and execute command */
    for (uint32_t i = 0; i < shell.count; i++) {
        if (strcmp(argv[0], shell.commands[i].name) == 0) {
            return shell.commands[i].handler(argc, argv);
        }
    }
    
    printf("Unknown command: %s (type 'help' for list)\n", argv[0]);
    return -1;
}

void os_shell_task(void *arg) {
    (void)arg;
    char line[OS_SHELL_LINE_MAX];
    
    LOG_I(SHELL_MODULE, "Shell started");
    printf("\n=== ESP32-C6 Zigbee Bridge Shell ===\n");
    printf("Type 'help' for available commands.\n\n");
    printf("> ");
    fflush(stdout);
    
    while (1) {
        /* Flush logs */
        os_log_flush();
        
        /* Check for input */
        int len = os_console_readline(line, sizeof(line), true);
        if (len >= 0) {
            os_shell_process(line);
            printf("> ");
            fflush(stdout);
        }
        
        /* Yield to other fibres */
        os_sleep(10);
    }
}

/* Built-in command implementations */

static int cmd_help(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Available commands:\n");
    for (uint32_t i = 0; i < shell.count; i++) {
        printf("  %-12s - %s\n", shell.commands[i].name, shell.commands[i].help);
    }
    return 0;
}

static int cmd_ps(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("%-4s %-12s %-10s %8s %8s %10s\n",
           "ID", "NAME", "STATE", "STACK", "USED", "RUNS");
    printf("---- ------------ ---------- -------- -------- ----------\n");
    
    const char *state_names[] = {"READY", "RUNNING", "SLEEPING", "BLOCKED", "DEAD"};
    
    uint32_t count = os_fibre_count();
    for (uint32_t i = 0; i < count; i++) {
        os_fibre_info_t info;
        if (os_fibre_get_info(i, &info) == OS_OK) {
            const char *state = (info.state < 5) ? state_names[info.state] : "?";
            printf("%-4u %-12s %-10s %8u %8u %10u\n",
                   i, info.name, state, info.stack_size, info.stack_used, info.run_count);
        }
    }
    
    return 0;
}

static int cmd_uptime(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    os_time_ms_t ms = os_uptime_ms();
    uint32_t secs = ms / 1000;
    uint32_t mins = secs / 60;
    uint32_t hours = mins / 60;
    
    printf("Uptime: %02u:%02u:%02u.%03u (%" PRIu32 " ticks)\n",
           hours, mins % 60, secs % 60, (uint32_t)(ms % 1000), os_now_ticks());
    
    return 0;
}

static int cmd_loglevel(int argc, char *argv[]) {
    if (argc > 1) {
        os_log_level_t level = os_log_level_parse(argv[1]);
        os_log_set_level(level);
        printf("Log level set to: %s\n", os_log_level_name(level));
    } else {
        printf("Current log level: %s\n", os_log_level_name(os_log_get_level()));
        printf("Available levels: ERROR, WARN, INFO, DEBUG, TRACE\n");
    }
    return 0;
}

static int cmd_stats(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    os_event_stats_t stats;
    if (os_event_get_stats(&stats) == OS_OK) {
        printf("Event Bus Statistics:\n");
        printf("  Published:    %u\n", stats.events_published);
        printf("  Dispatched:   %u\n", stats.events_dispatched);
        printf("  Dropped:      %u\n", stats.events_dropped);
        printf("  Queue size:   %u\n", stats.current_queue_size);
        printf("  High water:   %u\n", stats.queue_high_water);
    }
    return 0;
}

static int cmd_sched(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    os_sched_stats_t stats;
    if (os_fibre_get_stats(&stats) != OS_OK) {
        printf("Scheduler stats unavailable\n");
        return -1;
    }
    
    printf("Scheduler Stats:\n");
    printf("  Ticks:        %" PRIu32 "\n", (uint32_t)stats.ticks);
    printf("  Fibres:       %u\n", stats.fibre_count);
    printf("  Ready:        %u\n", stats.ready_count);
    printf("  Sleeping:     %u\n", stats.sleeping_count);
    
    printf("\n%-4s %-12s %-10s %8s %8s %10s %10s %10s\n",
           "ID", "NAME", "STATE", "STACK", "USED", "RUNS", "LAST", "TOTAL");
    printf("---- ------------ ---------- -------- -------- ---------- ---------- ----------\n");
    
    const char *state_names[] = {"READY", "RUNNING", "SLEEPING", "BLOCKED", "DEAD"};
    
    uint32_t count = os_fibre_count();
    for (uint32_t i = 0; i < count; i++) {
        os_fibre_info_t info;
        if (os_fibre_get_info(i, &info) == OS_OK) {
            const char *state = (info.state < 5) ? state_names[info.state] : "?";
            printf("%-4u %-12s %-10s %8u %8u %10u %10" PRIu32 " %10" PRIu32 "\n",
                   i, info.name, state, info.stack_size, info.stack_used, info.run_count,
                   (uint32_t)info.last_run_tick, (uint32_t)info.total_run_ticks);
        }
    }
    
    return 0;
}

static int cmd_events(int argc, char *argv[]) {
    return cmd_stats(argc, argv);
}

static int cmd_persist(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    os_persist_stats_t stats;
    os_persist_get_stats_ex(&stats);
    
    printf("Persistence Stats:\n");
    printf("  Buffered:     %u\n", stats.writes_buffered);
    printf("  Writes:       %u\n", stats.total_writes);
    printf("  Reads:        %u\n", stats.total_reads);
    printf("  Last flush:   %" PRIu32 "\n", (uint32_t)stats.last_flush_tick);
    printf("  Last error:   %d\n", stats.last_error);
    
    return 0;
}

static int cmd_mqtt(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    mqtt_stats_t stats;
    if (mqtt_get_stats(&stats) != OS_OK) {
        printf("MQTT stats unavailable\n");
        return -1;
    }
    
    printf("MQTT Stats:\n");
    printf("  State:        %s\n", mqtt_state_name(mqtt_get_state()));
    printf("  Published:    %u\n", stats.messages_published);
    printf("  Received:     %u\n", stats.messages_received);
    printf("  Reconnects:   %u\n", stats.reconnects);
    printf("  Errors:       %u\n", stats.errors);
    
    return 0;
}
